#!/usr/bin/env python3
"""Mosaic redemption and AI proxy gateway.

The gateway intentionally uses only the Python standard library so it can run
on a small Linux server without a separate application stack.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import secrets
import sqlite3
import sys
import threading
import time
import urllib.error
import urllib.request
from collections import defaultdict, deque
from contextlib import contextmanager
from dataclasses import dataclass
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any, Iterator
from urllib.parse import urlsplit


SCHEMA = """
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS redemption_codes (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    code_hash           TEXT NOT NULL UNIQUE,
    code_hint           TEXT NOT NULL,
    label               TEXT NOT NULL DEFAULT '',
    mode                TEXT NOT NULL CHECK (mode IN ('text', 'qwen_ocr')),
    upstream_api_base   TEXT NOT NULL,
    provider_api_key    TEXT NOT NULL,
    model               TEXT NOT NULL,
    ocr_cloud_mode      TEXT NOT NULL DEFAULT 'single'
                            CHECK (ocr_cloud_mode IN ('single', 'dual')),
    remaining_uses      INTEGER NOT NULL DEFAULT 0 CHECK (remaining_uses >= 0),
    redeemed_uses       INTEGER NOT NULL DEFAULT 0 CHECK (redeemed_uses >= 0),
    enabled             INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
    expires_at          TEXT,
    created_at          TEXT NOT NULL,
    updated_at          TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS activations (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    code_id             INTEGER NOT NULL REFERENCES redemption_codes(id)
                            ON DELETE CASCADE,
    client_id_hash      TEXT NOT NULL,
    token_hash          TEXT NOT NULL UNIQUE,
    enabled             INTEGER NOT NULL DEFAULT 1 CHECK (enabled IN (0, 1)),
    request_count       INTEGER NOT NULL DEFAULT 0 CHECK (request_count >= 0),
    created_at          TEXT NOT NULL,
    last_redeemed_at    TEXT NOT NULL,
    last_used_at        TEXT,
    UNIQUE (code_id, client_id_hash)
);

CREATE INDEX IF NOT EXISTS idx_activations_token_hash
    ON activations(token_hash);
"""

CHAT_SUFFIX = "/chat/completions"
OCR_SUFFIX = "/services/aigc/multimodal-generation/generation"
DEFAULT_MAX_BODY_BYTES = 20 * 1024 * 1024
DEFAULT_REDEEM_BODY_BYTES = 32 * 1024


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def normalize_code(code: str) -> str:
    return "".join(ch for ch in code.upper() if ch.isalnum())


def digest(value: str) -> str:
    return hashlib.sha256(value.encode("utf-8")).hexdigest()


def parse_expiry(value: str | None) -> datetime | None:
    if not value:
        return None
    normalized = value.strip().replace("Z", "+00:00")
    parsed = datetime.fromisoformat(normalized)
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return parsed.astimezone(timezone.utc)


def is_expired(value: str | None) -> bool:
    expiry = parse_expiry(value)
    return expiry is not None and expiry <= datetime.now(timezone.utc)


def validated_base_url(value: str) -> str:
    candidate = value.strip().rstrip("/")
    parsed = urlsplit(candidate)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        raise ValueError("API base URL must be an absolute http(s) URL")
    return candidate


class GatewayError(Exception):
    def __init__(self, code: str, message: str, status: int) -> None:
        super().__init__(message)
        self.code = code
        self.message = message
        self.status = status


@dataclass(frozen=True)
class AuthorizedConfig:
    activation_id: int
    mode: str
    upstream_api_base: str
    provider_api_key: str
    model: str


class GatewayStore:
    def __init__(self, database_path: str | Path) -> None:
        self.database_path = str(database_path)

    @contextmanager
    def connect(self) -> Iterator[sqlite3.Connection]:
        conn = sqlite3.connect(self.database_path, timeout=15, isolation_level=None)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON")
        conn.execute("PRAGMA busy_timeout = 15000")
        try:
            yield conn
        finally:
            conn.close()

    def initialize(self) -> None:
        path = Path(self.database_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as conn:
            conn.executescript(SCHEMA)
            conn.execute("PRAGMA journal_mode = WAL")

    def redeem(
        self,
        code: str,
        client_id: str,
        *,
        public_base_url: str,
    ) -> dict[str, Any]:
        normalized = normalize_code(code)
        client_id = client_id.strip()
        if len(normalized) < 8 or len(normalized) > 64:
            raise GatewayError("invalid_code", "The redemption code is invalid.", HTTPStatus.NOT_FOUND)
        if len(client_id) < 8 or len(client_id) > 128:
            raise GatewayError("invalid_request", "The client identifier is invalid.", HTTPStatus.BAD_REQUEST)

        code_hash = digest(normalized)
        client_hash = digest(client_id)
        now = utc_now()
        activation_token = "mosaic_at_" + secrets.token_urlsafe(32)
        token_hash = digest(activation_token)

        with self.connect() as conn:
            try:
                conn.execute("BEGIN IMMEDIATE")
                row = conn.execute(
                    "SELECT * FROM redemption_codes WHERE code_hash = ?",
                    (code_hash,),
                ).fetchone()
                if row is None:
                    raise GatewayError(
                        "invalid_code", "The redemption code is invalid.", HTTPStatus.NOT_FOUND
                    )
                if not row["enabled"]:
                    raise GatewayError(
                        "disabled_code", "The redemption code is disabled.", HTTPStatus.FORBIDDEN
                    )
                if is_expired(row["expires_at"]):
                    raise GatewayError(
                        "expired_code", "The redemption code has expired.", HTTPStatus.GONE
                    )

                activation = conn.execute(
                    """
                    SELECT * FROM activations
                    WHERE code_id = ? AND client_id_hash = ?
                    """,
                    (row["id"], client_hash),
                ).fetchone()
                already_redeemed = activation is not None

                if activation is not None:
                    if not activation["enabled"]:
                        raise GatewayError(
                            "revoked_activation",
                            "This activation has been revoked.",
                            HTTPStatus.FORBIDDEN,
                        )
                    conn.execute(
                        """
                        UPDATE activations
                        SET token_hash = ?, last_redeemed_at = ?
                        WHERE id = ?
                        """,
                        (token_hash, now, activation["id"]),
                    )
                else:
                    if row["remaining_uses"] <= 0:
                        raise GatewayError(
                            "code_exhausted",
                            "The redemption code has no remaining activations.",
                            HTTPStatus.CONFLICT,
                        )
                    conn.execute(
                        """
                        UPDATE redemption_codes
                        SET remaining_uses = remaining_uses - 1,
                            redeemed_uses = redeemed_uses + 1,
                            updated_at = ?
                        WHERE id = ?
                        """,
                        (now, row["id"]),
                    )
                    conn.execute(
                        """
                        INSERT INTO activations(
                            code_id, client_id_hash, token_hash, enabled,
                            request_count, created_at, last_redeemed_at
                        ) VALUES (?, ?, ?, 1, 0, ?, ?)
                        """,
                        (row["id"], client_hash, token_hash, now, now),
                    )

                current = conn.execute(
                    "SELECT * FROM redemption_codes WHERE id = ?",
                    (row["id"],),
                ).fetchone()
                conn.commit()
            except Exception:
                conn.rollback()
                raise

        mode = str(current["mode"])
        gateway_path = "/api/v1" if mode == "qwen_ocr" else "/v1"
        return {
            "ok": True,
            "config": {
                "mode": mode,
                "api_base": public_base_url.rstrip("/") + gateway_path,
                "api_key": activation_token,
                "model": current["model"],
                "ocr_cloud_mode": current["ocr_cloud_mode"],
                "managed_gateway": True,
            },
            "redemption": {
                "label": current["label"],
                "remaining_uses": current["remaining_uses"],
                "already_redeemed": already_redeemed,
            },
        }

    def authorize(self, token: str, required_mode: str) -> AuthorizedConfig:
        token = token.strip()
        if not token.startswith("mosaic_at_") or len(token) < 32:
            raise GatewayError("unauthorized", "Invalid gateway token.", HTTPStatus.UNAUTHORIZED)

        with self.connect() as conn:
            row = conn.execute(
                """
                SELECT
                    a.id AS activation_id,
                    a.enabled AS activation_enabled,
                    c.enabled AS code_enabled,
                    c.expires_at,
                    c.mode,
                    c.upstream_api_base,
                    c.provider_api_key,
                    c.model
                FROM activations a
                JOIN redemption_codes c ON c.id = a.code_id
                WHERE a.token_hash = ?
                """,
                (digest(token),),
            ).fetchone()

        if row is None:
            raise GatewayError("unauthorized", "Invalid gateway token.", HTTPStatus.UNAUTHORIZED)
        if not row["activation_enabled"] or not row["code_enabled"]:
            raise GatewayError("unauthorized", "This gateway token is disabled.", HTTPStatus.UNAUTHORIZED)
        if is_expired(row["expires_at"]):
            raise GatewayError("unauthorized", "This gateway token has expired.", HTTPStatus.UNAUTHORIZED)
        if row["mode"] != required_mode:
            raise GatewayError(
                "wrong_mode", "This token cannot use the requested AI route.", HTTPStatus.FORBIDDEN
            )

        return AuthorizedConfig(
            activation_id=int(row["activation_id"]),
            mode=str(row["mode"]),
            upstream_api_base=str(row["upstream_api_base"]),
            provider_api_key=str(row["provider_api_key"]),
            model=str(row["model"]),
        )

    def record_request(self, activation_id: int) -> None:
        with self.connect() as conn:
            conn.execute(
                """
                UPDATE activations
                SET request_count = request_count + 1, last_used_at = ?
                WHERE id = ?
                """,
                (utc_now(), activation_id),
            )


class SlidingWindowLimiter:
    def __init__(self, limit: int, window_seconds: int) -> None:
        self.limit = limit
        self.window_seconds = window_seconds
        self._events: dict[str, deque[float]] = defaultdict(deque)
        self._lock = threading.Lock()

    def allow(self, key: str) -> bool:
        now = time.monotonic()
        cutoff = now - self.window_seconds
        with self._lock:
            events = self._events[key]
            while events and events[0] < cutoff:
                events.popleft()
            if len(events) >= self.limit:
                return False
            events.append(now)
            return True


class GatewayApplication:
    def __init__(
        self,
        store: GatewayStore,
        *,
        public_base_url: str | None,
        upstream_timeout_seconds: int,
        max_body_bytes: int,
        redeem_rate_limit: int,
    ) -> None:
        self.store = store
        self.public_base_url = public_base_url.rstrip("/") if public_base_url else None
        self.upstream_timeout_seconds = upstream_timeout_seconds
        self.max_body_bytes = max_body_bytes
        self.redeem_limiter = SlidingWindowLimiter(redeem_rate_limit, 60)

    def public_url_for(self, handler: BaseHTTPRequestHandler) -> str:
        if self.public_base_url:
            return self.public_base_url
        host = handler.headers.get("Host", "").strip()
        if not host:
            raise GatewayError(
                "server_misconfigured",
                "The gateway public URL is not configured.",
                HTTPStatus.INTERNAL_SERVER_ERROR,
            )
        return validated_base_url("http://" + host)

    def redeem(self, handler: BaseHTTPRequestHandler, payload: dict[str, Any]) -> dict[str, Any]:
        ip = handler.client_address[0]
        if not self.redeem_limiter.allow(ip):
            raise GatewayError(
                "rate_limited",
                "Too many redemption attempts. Please try again later.",
                HTTPStatus.TOO_MANY_REQUESTS,
            )
        return self.store.redeem(
            str(payload.get("code", "")),
            str(payload.get("client_id", "")),
            public_base_url=self.public_url_for(handler),
        )

    def proxy(
        self,
        *,
        token: str,
        required_mode: str,
        body: bytes,
    ) -> tuple[int, bytes, str]:
        config = self.store.authorize(token, required_mode)
        try:
            parsed = json.loads(body.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            raise GatewayError(
                "invalid_request", "The upstream request must be valid JSON.", HTTPStatus.BAD_REQUEST
            )
        if not isinstance(parsed, dict):
            raise GatewayError(
                "invalid_request", "The upstream request must be a JSON object.", HTTPStatus.BAD_REQUEST
            )

        parsed["model"] = config.model
        if required_mode == "text":
            parsed["stream"] = False
            suffix = CHAT_SUFFIX
        else:
            suffix = OCR_SUFFIX
        upstream_url = config.upstream_api_base.rstrip("/") + suffix
        print(
            f"proxy {required_mode} -> {upstream_url} "
            f"(timeout={self.upstream_timeout_seconds}s, body={len(body)} bytes)",
            flush=True,
        )
        upstream_body = json.dumps(parsed, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        request = urllib.request.Request(
            upstream_url,
            data=upstream_body,
            method="POST",
            headers={
                "Authorization": "Bearer " + config.provider_api_key,
                "Content-Type": "application/json",
                "Accept": "application/json",
                "User-Agent": "Mosaic-Redemption-Gateway/1.0",
            },
        )

        try:
            with urllib.request.urlopen(
                request, timeout=self.upstream_timeout_seconds
            ) as response:
                status = int(response.status)
                response_body = response.read(self.max_body_bytes + 1)
                content_type = response.headers.get("Content-Type", "application/json")
        except urllib.error.HTTPError as exc:
            status = int(exc.code)
            response_body = exc.read(self.max_body_bytes + 1)
            content_type = exc.headers.get("Content-Type", "application/json")
            print(
                f"upstream HTTP {status} for {required_mode}: {upstream_url}",
                flush=True,
            )
        except (urllib.error.URLError, TimeoutError) as exc:
            reason = exc.reason if hasattr(exc, "reason") else exc
            print(
                f"upstream unavailable for {required_mode}: {upstream_url} ({reason})",
                flush=True,
            )
            raise GatewayError(
                "upstream_unavailable",
                f"The configured AI provider is unavailable: {reason}",
                HTTPStatus.BAD_GATEWAY,
            )

        self.store.record_request(config.activation_id)
        if len(response_body) > self.max_body_bytes:
            raise GatewayError(
                "upstream_response_too_large",
                "The AI provider response is too large.",
                HTTPStatus.BAD_GATEWAY,
            )
        return status, response_body, content_type


class GatewayHandler(BaseHTTPRequestHandler):
    server_version = "MosaicGateway/1.0"
    sys_version = ""

    @property
    def app(self) -> GatewayApplication:
        return self.server.app  # type: ignore[attr-defined]

    def log_message(self, fmt: str, *args: Any) -> None:
        safe_path = self.path.split("?", 1)[0]
        print(
            f"{self.log_date_time_string()} {self.client_address[0]} "
            f"{self.command} {safe_path} {fmt % args}",
            flush=True,
        )

    def _send_json(self, status: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        self._send_bytes(status, body, "application/json; charset=utf-8")

    def _send_bytes(self, status: int, body: bytes, content_type: str) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def _read_body(self, limit: int) -> bytes:
        raw_length = self.headers.get("Content-Length", "")
        try:
            length = int(raw_length)
        except ValueError:
            raise GatewayError(
                "invalid_request", "Content-Length is required.", HTTPStatus.LENGTH_REQUIRED
            )
        if length < 0 or length > limit:
            raise GatewayError(
                "request_too_large", "The request body is too large.", HTTPStatus.REQUEST_ENTITY_TOO_LARGE
            )
        return self.rfile.read(length)

    def _bearer_token(self) -> str:
        value = self.headers.get("Authorization", "")
        if not value.lower().startswith("bearer "):
            raise GatewayError(
                "unauthorized", "A gateway bearer token is required.", HTTPStatus.UNAUTHORIZED
            )
        return value[7:].strip()

    def do_GET(self) -> None:
        if self.path == "/":
            body = (
                "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<title>Mosaic Gateway</title></head>"
                "<body style=\"font-family:Segoe UI,Microsoft YaHei,sans-serif;"
                "max-width:720px;margin:64px auto;padding:0 24px;color:#134e4a\">"
                "<h1>Mosaic 兑换码网关</h1>"
                "<p>服务运行正常。</p>"
                "<p><a href=\"/healthz\">查看健康检查</a></p>"
                "</body></html>"
            ).encode("utf-8")
            self._send_bytes(HTTPStatus.OK, body, "text/html; charset=utf-8")
            return
        if self.path == "/favicon.ico":
            self._send_bytes(HTTPStatus.NO_CONTENT, b"", "image/x-icon")
            return
        if self.path == "/healthz":
            self._send_json(HTTPStatus.OK, {"ok": True})
            return
        self._send_json(
            HTTPStatus.NOT_FOUND,
            {"ok": False, "error": {"code": "not_found", "message": "Not found."}},
        )

    def do_POST(self) -> None:
        try:
            if self.path == "/api/v1/redeem":
                body = self._read_body(DEFAULT_REDEEM_BODY_BYTES)
                try:
                    payload = json.loads(body.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError):
                    raise GatewayError(
                        "invalid_request", "The request must be valid JSON.", HTTPStatus.BAD_REQUEST
                    )
                if not isinstance(payload, dict):
                    raise GatewayError(
                        "invalid_request",
                        "The request must be a JSON object.",
                        HTTPStatus.BAD_REQUEST,
                    )
                self._send_json(HTTPStatus.OK, self.app.redeem(self, payload))
                return

            if self.path == "/v1/chat/completions":
                body = self._read_body(self.app.max_body_bytes)
                status, response, content_type = self.app.proxy(
                    token=self._bearer_token(),
                    required_mode="text",
                    body=body,
                )
                self._send_bytes(status, response, content_type)
                return

            if self.path == "/api/v1" + OCR_SUFFIX:
                body = self._read_body(self.app.max_body_bytes)
                status, response, content_type = self.app.proxy(
                    token=self._bearer_token(),
                    required_mode="qwen_ocr",
                    body=body,
                )
                self._send_bytes(status, response, content_type)
                return

            raise GatewayError("not_found", "Not found.", HTTPStatus.NOT_FOUND)
        except GatewayError as exc:
            self._send_json(
                exc.status,
                {"ok": False, "error": {"code": exc.code, "message": exc.message}},
            )
        except Exception as exc:
            import traceback

            print(f"gateway internal error on {self.path}: {exc}", flush=True)
            print(traceback.format_exc(), flush=True)
            self._send_json(
                HTTPStatus.INTERNAL_SERVER_ERROR,
                {
                    "ok": False,
                    "error": {
                        "code": "internal_error",
                        "message": "The gateway encountered an internal error.",
                    },
                },
            )


class BoundedThreadingHTTPServer(ThreadingHTTPServer):
    daemon_threads = True
    request_queue_size = 64

    def __init__(self, server_address: tuple[str, int], handler_class: type[BaseHTTPRequestHandler],
                 max_workers: int) -> None:
        self._worker_slots = threading.BoundedSemaphore(max(1, max_workers))
        super().__init__(server_address, handler_class)

    def process_request(self, request: Any, client_address: Any) -> None:
        self._worker_slots.acquire()
        try:
            super().process_request(request, client_address)
        except Exception:
            self._worker_slots.release()
            raise

    def process_request_thread(self, request: Any, client_address: Any) -> None:
        try:
            super().process_request_thread(request, client_address)
        finally:
            self._worker_slots.release()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run the Mosaic redemption gateway")
    parser.add_argument(
        "--host", default=os.getenv("MOSAIC_GATEWAY_HOST", "127.0.0.1")
    )
    parser.add_argument(
        "--port", type=int, default=int(os.getenv("MOSAIC_GATEWAY_PORT", "8787"))
    )
    parser.add_argument(
        "--database",
        default=os.getenv(
            "MOSAIC_GATEWAY_DB",
            str(Path(__file__).resolve().parent / "data" / "gateway.sqlite3"),
        ),
    )
    parser.add_argument(
        "--public-base-url",
        default=os.getenv("MOSAIC_GATEWAY_PUBLIC_URL") or None,
        help="Public URL returned to clients, for example http://1.2.3.4:8787",
    )
    parser.add_argument(
        "--upstream-timeout",
        type=int,
        default=int(os.getenv("MOSAIC_GATEWAY_UPSTREAM_TIMEOUT", "150")),
    )
    parser.add_argument(
        "--max-body-mb",
        type=int,
        default=int(os.getenv("MOSAIC_GATEWAY_MAX_BODY_MB", "20")),
    )
    parser.add_argument(
        "--redeem-attempts-per-minute",
        type=int,
        default=int(os.getenv("MOSAIC_GATEWAY_REDEEM_RATE", "20")),
    )
    parser.add_argument(
        "--max-workers",
        type=int,
        default=int(os.getenv("MOSAIC_GATEWAY_MAX_WORKERS", "8")),
        help="Maximum concurrent HTTP requests",
    )
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.public_base_url:
        args.public_base_url = validated_base_url(args.public_base_url)
    store = GatewayStore(args.database)
    store.initialize()
    app = GatewayApplication(
        store,
        public_base_url=args.public_base_url,
        upstream_timeout_seconds=max(5, args.upstream_timeout),
        max_body_bytes=max(1, args.max_body_mb) * 1024 * 1024,
        redeem_rate_limit=max(1, args.redeem_attempts_per_minute),
    )
    server = BoundedThreadingHTTPServer(
        (args.host, args.port), GatewayHandler, max(1, args.max_workers)
    )
    server.app = app  # type: ignore[attr-defined]
    print(
        f"Mosaic redemption gateway listening on {args.host}:{args.port}; "
        f"database={Path(args.database).resolve()}; "
        f"upstream_timeout={args.upstream_timeout}s",
        flush=True,
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
