from __future__ import annotations

import json
import tempfile
import unittest
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from unittest.mock import patch

import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from gateway import (
    GatewayApplication,
    GatewayError,
    GatewayStore,
    digest,
    normalize_code,
    utc_now,
)


class GatewayStoreTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temp_dir.name) / "gateway.sqlite3"
        self.store = GatewayStore(self.db_path)
        self.store.initialize()
        self.code = "MOS-ABCD-EFGH-JKLM"
        now = utc_now()
        with self.store.connect() as conn:
            conn.execute(
                """
                INSERT INTO redemption_codes(
                    code_hash, code_hint, label, mode, upstream_api_base,
                    provider_api_key, model, ocr_cloud_mode, remaining_uses,
                    redeemed_uses, enabled, created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 1, ?, ?)
                """,
                (
                    digest(normalize_code(self.code)),
                    "JKLM",
                    "Test",
                    "qwen_ocr",
                    "https://dashscope.aliyuncs.com/api/v1",
                    "provider-secret",
                    "qwen3.5-ocr",
                    "single",
                    2,
                    now,
                    now,
                ),
            )

    def tearDown(self) -> None:
        self.temp_dir.cleanup()

    def test_first_device_consumes_once_and_retry_is_idempotent(self) -> None:
        first = self.store.redeem(
            self.code, "client-one-1234", public_base_url="http://gateway.test:8787"
        )
        again = self.store.redeem(
            self.code, "client-one-1234", public_base_url="http://gateway.test:8787"
        )

        self.assertEqual(first["redemption"]["remaining_uses"], 1)
        self.assertFalse(first["redemption"]["already_redeemed"])
        self.assertEqual(again["redemption"]["remaining_uses"], 1)
        self.assertTrue(again["redemption"]["already_redeemed"])
        self.assertNotEqual(first["config"]["api_key"], again["config"]["api_key"])
        self.assertNotIn("provider-secret", json.dumps(first))

    def test_new_devices_exhaust_remaining_uses(self) -> None:
        self.store.redeem(
            self.code, "client-one-1234", public_base_url="http://gateway.test:8787"
        )
        self.store.redeem(
            self.code, "client-two-1234", public_base_url="http://gateway.test:8787"
        )
        with self.assertRaises(GatewayError) as caught:
            self.store.redeem(
                self.code, "client-three-1234", public_base_url="http://gateway.test:8787"
            )
        self.assertEqual(caught.exception.code, "code_exhausted")

    def test_authorization_uses_current_server_side_key(self) -> None:
        redeemed = self.store.redeem(
            self.code, "client-one-1234", public_base_url="http://gateway.test:8787"
        )
        token = redeemed["config"]["api_key"]
        authorized = self.store.authorize(token, "qwen_ocr")
        self.assertEqual(authorized.provider_api_key, "provider-secret")

        with self.store.connect() as conn:
            conn.execute(
                "UPDATE redemption_codes SET provider_api_key = ?",
                ("rotated-secret",),
            )
        authorized_after_rotation = self.store.authorize(token, "qwen_ocr")
        self.assertEqual(authorized_after_rotation.provider_api_key, "rotated-secret")

    def test_token_cannot_use_wrong_route(self) -> None:
        redeemed = self.store.redeem(
            self.code, "client-one-1234", public_base_url="http://gateway.test:8787"
        )
        with self.assertRaises(GatewayError) as caught:
            self.store.authorize(redeemed["config"]["api_key"], "text")
        self.assertEqual(caught.exception.code, "wrong_mode")

    def test_last_activation_is_consumed_atomically(self) -> None:
        with self.store.connect() as conn:
            conn.execute(
                "UPDATE redemption_codes SET remaining_uses = 1 WHERE code_hash = ?",
                (digest(normalize_code(self.code)),),
            )

        def redeem(client_id: str) -> str:
            try:
                self.store.redeem(
                    self.code, client_id, public_base_url="http://gateway.test:8787"
                )
                return "ok"
            except GatewayError as exc:
                return exc.code

        with ThreadPoolExecutor(max_workers=2) as executor:
            results = list(
                executor.map(redeem, ("concurrent-client-one", "concurrent-client-two"))
            )
        self.assertEqual(sorted(results), ["code_exhausted", "ok"])

    def test_proxy_injects_server_key_and_enforces_model(self) -> None:
        redeemed = self.store.redeem(
            self.code, "client-one-1234", public_base_url="http://gateway.test:8787"
        )
        token = redeemed["config"]["api_key"]
        app = GatewayApplication(
            self.store,
            public_base_url="http://gateway.test:8787",
            upstream_timeout_seconds=10,
            max_body_bytes=1024 * 1024,
            redeem_rate_limit=20,
        )
        captured = {}

        class FakeResponse:
            status = 200
            headers = {"Content-Type": "application/json"}

            def __enter__(self):
                return self

            def __exit__(self, *_args):
                return False

            def read(self, _limit: int) -> bytes:
                return b'{"output":{"choices":[]}}'

        def fake_urlopen(request, timeout):
            captured["url"] = request.full_url
            captured["headers"] = dict(request.header_items())
            captured["body"] = json.loads(request.data.decode("utf-8"))
            captured["timeout"] = timeout
            return FakeResponse()

        with patch("gateway.urllib.request.urlopen", side_effect=fake_urlopen):
            status, response, _content_type = app.proxy(
                token=token,
                required_mode="qwen_ocr",
                body=b'{"model":"unauthorized-model","input":{}}',
            )

        self.assertEqual(status, 200)
        self.assertEqual(response, b'{"output":{"choices":[]}}')
        self.assertEqual(captured["body"]["model"], "qwen3.5-ocr")
        self.assertEqual(captured["headers"]["Authorization"], "Bearer provider-secret")
        self.assertTrue(captured["url"].endswith("/api/v1/services/aigc/multimodal-generation/generation"))


if __name__ == "__main__":
    unittest.main()
