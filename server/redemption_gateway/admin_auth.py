from __future__ import annotations

import base64
import hashlib
import json
import secrets
import threading
import time
from collections import defaultdict, deque
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


PBKDF2_ITERATIONS = 600_000
SESSION_COOKIE = "mosaic_admin_session"
SESSION_TTL_SECONDS = 12 * 60 * 60
LOGIN_WINDOW_SECONDS = 15 * 60
LOGIN_MAX_ATTEMPTS = 10


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def normalize_base_path(value: str) -> str:
    trimmed = value.strip()
    if not trimmed or trimmed == "/":
        return ""
    if not trimmed.startswith("/"):
        trimmed = "/" + trimmed
    return trimmed.rstrip("/")


def join_base_path(base_path: str, path: str) -> str:
    normalized = normalize_base_path(base_path)
    if not path.startswith("/"):
        path = "/" + path
    if not normalized:
        return path
    if path == "/":
        return normalized + "/"
    return normalized + path


def hash_password(password: str, *, salt: bytes | None = None) -> dict[str, str]:
    if salt is None:
        salt = secrets.token_bytes(16)
    digest = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode("utf-8"),
        salt,
        PBKDF2_ITERATIONS,
    )
    return {
        "algorithm": "pbkdf2_sha256",
        "iterations": str(PBKDF2_ITERATIONS),
        "salt": base64.b64encode(salt).decode("ascii"),
        "hash": base64.b64encode(digest).decode("ascii"),
    }


def verify_password(password: str, record: dict[str, str]) -> bool:
    if record.get("algorithm") != "pbkdf2_sha256":
        return False
    try:
        iterations = int(record.get("iterations", "0"))
        salt = base64.b64decode(record["salt"].encode("ascii"))
        expected = base64.b64decode(record["hash"].encode("ascii"))
    except (KeyError, TypeError, ValueError):
        return False
    if iterations <= 0 or not salt or not expected:
        return False
    actual = hashlib.pbkdf2_hmac(
        "sha256",
        password.encode("utf-8"),
        salt,
        iterations,
    )
    return secrets.compare_digest(actual, expected)


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


@dataclass(frozen=True)
class AdminCredentials:
    username: str
    password: dict[str, str]
    updated_at: str


class AdminAuthManager:
    def __init__(self, credentials_path: str | Path) -> None:
        self.credentials_path = Path(credentials_path)
        self._lock = threading.Lock()
        self._sessions: dict[str, float] = {}
        self._login_limiter = SlidingWindowLimiter(LOGIN_MAX_ATTEMPTS, LOGIN_WINDOW_SECONDS)

    def credentials_configured(self) -> bool:
        return self.credentials_path.is_file()

    def load_credentials(self) -> AdminCredentials | None:
        if not self.credentials_path.is_file():
            return None
        payload = json.loads(self.credentials_path.read_text(encoding="utf-8"))
        username = str(payload.get("username", "")).strip()
        password = payload.get("password")
        updated_at = str(payload.get("updated_at", "")).strip()
        if not username or not isinstance(password, dict):
            return None
        return AdminCredentials(username=username, password=password, updated_at=updated_at)

    def set_password(self, username: str, password: str) -> None:
        username = username.strip()
        if len(username) < 3:
            raise ValueError("Username must be at least 3 characters.")
        if len(password) < 12:
            raise ValueError("Password must be at least 12 characters.")
        record = {
            "username": username,
            "password": hash_password(password),
            "updated_at": utc_now(),
        }
        self.credentials_path.parent.mkdir(parents=True, exist_ok=True)
        temp_path = self.credentials_path.with_suffix(".tmp")
        temp_path.write_text(
            json.dumps(record, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        temp_path.replace(self.credentials_path)
        with self._lock:
            self._sessions.clear()

    def login_allowed(self, client_ip: str) -> bool:
        return self._login_limiter.allow(client_ip or "unknown")

    def authenticate(self, username: str, password: str) -> bool:
        credentials = self.load_credentials()
        if credentials is None:
            return False
        if not secrets.compare_digest(username.strip(), credentials.username):
            return False
        return verify_password(password, credentials.password)

    def create_session(self) -> str:
        token = secrets.token_urlsafe(32)
        expires_at = time.monotonic() + SESSION_TTL_SECONDS
        with self._lock:
            self._purge_expired_sessions_locked(time.monotonic())
            self._sessions[token] = expires_at
        return token

    def destroy_session(self, token: str) -> None:
        if not token:
            return
        with self._lock:
            self._sessions.pop(token, None)

    def session_valid(self, token: str) -> bool:
        if not token:
            return False
        now = time.monotonic()
        with self._lock:
            self._purge_expired_sessions_locked(now)
            expires_at = self._sessions.get(token)
            if expires_at is None:
                return False
            if expires_at <= now:
                self._sessions.pop(token, None)
                return False
            return True

    def _purge_expired_sessions_locked(self, now: float) -> None:
        expired = [token for token, expiry in self._sessions.items() if expiry <= now]
        for token in expired:
            self._sessions.pop(token, None)
