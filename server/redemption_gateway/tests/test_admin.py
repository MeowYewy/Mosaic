from __future__ import annotations

import http.cookiejar
import re
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from admin_auth import AdminAuthManager
from admin_service import (
    create_redemption_code,
    get_redemption_code,
    list_activations,
    list_redemption_codes,
    set_activation_enabled,
    update_redemption_code,
)
from admin_web import build_server
from gateway import GatewayStore


ADMIN_USERNAME = "admin"
ADMIN_PASSWORD = "test-password-12"


class AdminServiceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp_dir.name) / "gateway.sqlite3"
        self.store = GatewayStore(self.db_path)
        self.store.initialize()

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_create_edit_rotate_key_and_manage_activation(self) -> None:
        code_id, visible_code = create_redemption_code(
            self.store,
            code=None,
            label="后台测试",
            mode="qwen_ocr",
            api_base="https://dashscope.aliyuncs.com/api/v1/",
            provider_api_key="secret-one",
            model="qwen3.5-ocr",
            ocr_cloud_mode="single",
            remaining_uses=2,
            expires_at=None,
        )
        self.assertTrue(visible_code.startswith("MOS-"))
        rows = list_redemption_codes(self.store)
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["remaining_uses"], 2)
        self.assertNotIn("provider_api_key", rows[0])

        update_redemption_code(
            self.store,
            code_id,
            {
                "label": "修改后的备注",
                "remaining_uses": 5,
                "provider_api_key": "secret-two",
                "enabled": True,
            },
        )
        edited = get_redemption_code(self.store, code_id)
        self.assertEqual(edited["label"], "修改后的备注")
        self.assertEqual(edited["remaining_uses"], 5)
        self.assertNotIn("provider_api_key", edited)

        redeemed = self.store.redeem(
            visible_code,
            "admin-test-client",
            public_base_url="http://gateway.test",
        )
        authorization = self.store.authorize(
            redeemed["config"]["api_key"],
            "qwen_ocr",
        )
        self.assertEqual(authorization.provider_api_key, "secret-two")

        activation = list_activations(self.store, code_id)[0]
        returned_code_id = set_activation_enabled(
            self.store,
            activation["id"],
            False,
        )
        self.assertEqual(returned_code_id, code_id)
        self.assertEqual(list_activations(self.store, code_id)[0]["enabled"], 0)


class AdminWebTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp_dir.name) / "gateway.sqlite3"
        self.creds_path = Path(self.temp_dir.name) / "admin_credentials.json"
        auth = AdminAuthManager(self.creds_path)
        auth.set_password(ADMIN_USERNAME, ADMIN_PASSWORD)
        self.server = build_server(
            str(self.db_path),
            "127.0.0.1",
            0,
            credentials_path=str(self.creds_path),
        )
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.server.server_port}"
        self.cookies = http.cookiejar.CookieJar()
        self.opener = urllib.request.build_opener(
            urllib.request.HTTPCookieProcessor(self.cookies)
        )

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=3)
        self.temp_dir.cleanup()

    def get_text(self, path: str, *, follow_redirects: bool = True) -> tuple[int, str]:
        request = urllib.request.Request(self.base_url + path)
        if not follow_redirects:
            class NoRedirect(urllib.request.HTTPRedirectHandler):
                def redirect_request(self, req, fp, code, msg, headers, newurl):
                    return None

            opener = urllib.request.build_opener(
                urllib.request.HTTPCookieProcessor(self.cookies),
                NoRedirect(),
            )
            try:
                with opener.open(request, timeout=5) as response:
                    return response.status, response.read().decode("utf-8")
            except urllib.error.HTTPError as exc:
                return exc.code, exc.read().decode("utf-8")
        with self.opener.open(request, timeout=5) as response:
            return response.status, response.read().decode("utf-8")

    def post_form(self, path: str, fields: dict[str, str]) -> tuple[int, str]:
        request = urllib.request.Request(
            self.base_url + path,
            data=urllib.parse.urlencode(fields).encode("utf-8"),
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            method="POST",
        )
        try:
            with self.opener.open(request, timeout=5) as response:
                return response.status, response.read().decode("utf-8")
        except urllib.error.HTTPError as exc:
            return exc.code, exc.read().decode("utf-8")

    def csrf_token_from(self, html: str) -> str:
        token_match = re.search(r'name="csrf_token" value="([^"]+)"', html)
        self.assertIsNotNone(token_match)
        return token_match.group(1)

    def login(self) -> None:
        status, login_page = self.get_text("/login")
        self.assertEqual(status, 200)
        status, _ = self.post_form(
            "/login",
            {
                "csrf_token": self.csrf_token_from(login_page),
                "username": ADMIN_USERNAME,
                "password": ADMIN_PASSWORD,
            },
        )
        self.assertIn(status, (200, 303))
        status, body = self.get_text("/codes")
        self.assertEqual(status, 200)
        self.assertIn("兑换码", body)

    def test_unauthenticated_codes_redirects_to_login(self) -> None:
        status, _ = self.get_text("/codes", follow_redirects=False)
        self.assertEqual(status, 303)

    def test_login_rejects_invalid_password(self) -> None:
        status, login_page = self.get_text("/login")
        status, body = self.post_form(
            "/login",
            {
                "csrf_token": self.csrf_token_from(login_page),
                "username": ADMIN_USERNAME,
                "password": "wrong-password-12",
            },
        )
        self.assertEqual(status, 200)
        self.assertIn("用户名或密码不正确", body)

    def test_browser_create_flow_never_displays_provider_key(self) -> None:
        self.login()
        status, create_page = self.get_text("/codes/new")
        self.assertEqual(status, 200)
        token = self.csrf_token_from(create_page)

        status, success_page = self.post_form(
            "/codes",
            {
                "csrf_token": token,
                "code": "MOS-WEB1-TEST-0001",
                "label": "网页创建",
                "mode": "qwen_ocr",
                "api_base": "https://dashscope.aliyuncs.com/api/v1",
                "model": "qwen3.5-ocr",
                "ocr_cloud_mode": "single",
                "remaining_uses": "3",
                "expires_at": "",
                "provider_api_key": "browser-provider-secret",
                "provider_api_key_confirm": "browser-provider-secret",
            },
        )
        self.assertEqual(status, 201)
        self.assertIn("MOS-WEB1-TEST-0001", success_page)
        self.assertNotIn("browser-provider-secret", success_page)

        status, list_page = self.get_text("/codes")
        self.assertEqual(status, 200)
        self.assertIn("网页创建", list_page)
        self.assertNotIn("browser-provider-secret", list_page)

    def test_post_rejects_invalid_csrf_token(self) -> None:
        self.login()
        request = urllib.request.Request(
            self.base_url + "/codes",
            data=urllib.parse.urlencode({"csrf_token": "wrong"}).encode("utf-8"),
            headers={"Content-Type": "application/x-www-form-urlencoded"},
            method="POST",
        )
        with self.assertRaises(urllib.error.HTTPError) as caught:
            self.opener.open(request, timeout=5)
        self.assertEqual(caught.exception.code, 400)


if __name__ == "__main__":
    unittest.main()
