"""Shared administration operations for the CLI and local web panel."""

from __future__ import annotations

import secrets
import sqlite3
from collections.abc import Mapping
from typing import Any

from gateway import GatewayStore, digest, normalize_code, parse_expiry, utc_now, validated_base_url


ALPHABET = "23456789ABCDEFGHJKLMNPQRSTUVWXYZ"
ALLOWED_MODES = {"text", "qwen_ocr"}
ALLOWED_OCR_MODES = {"single", "dual"}


class AdminValidationError(ValueError):
    """A user-correctable administration input error."""


def generated_code() -> str:
    groups = ["".join(secrets.choice(ALPHABET) for _ in range(4)) for _ in range(3)]
    return "MOS-" + "-".join(groups)


def _validated_expiry(value: object) -> str | None:
    expiry = str(value or "").strip() or None
    try:
        parse_expiry(expiry)
    except (TypeError, ValueError) as exc:
        raise AdminValidationError("到期时间格式不正确。") from exc
    return expiry


def _validated_nonnegative_integer(value: object, field_name: str) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError) as exc:
        raise AdminValidationError(f"{field_name}必须是整数。") from exc
    if parsed < 0:
        raise AdminValidationError(f"{field_name}不能小于 0。")
    return parsed


def create_redemption_code(
    store: GatewayStore,
    *,
    code: str | None,
    label: str,
    mode: str,
    api_base: str,
    provider_api_key: str,
    model: str,
    ocr_cloud_mode: str,
    remaining_uses: int,
    expires_at: str | None,
) -> tuple[int, str]:
    visible_code = code.strip() if code and code.strip() else generated_code()
    normalized = normalize_code(visible_code)
    if len(normalized) < 8 or len(normalized) > 64:
        raise AdminValidationError("兑换码去除分隔符后必须为 8–64 个字母或数字。")
    if mode not in ALLOWED_MODES:
        raise AdminValidationError("调用模式不正确。")
    if ocr_cloud_mode not in ALLOWED_OCR_MODES:
        raise AdminValidationError("OCR 云端模式不正确。")

    clean_model = model.strip()
    if not clean_model:
        raise AdminValidationError("模型名称不能为空。")
    clean_key = provider_api_key.strip()
    if not clean_key:
        raise AdminValidationError("API Key 不能为空。")
    uses = _validated_nonnegative_integer(remaining_uses, "可用次数")
    expiry = _validated_expiry(expires_at)
    try:
        clean_api_base = validated_base_url(api_base)
    except ValueError as exc:
        raise AdminValidationError("API 地址必须是完整的 http:// 或 https:// 地址。") from exc

    now = utc_now()
    try:
        with store.connect() as conn:
            cursor = conn.execute(
                """
                INSERT INTO redemption_codes(
                    code_hash, code_hint, label, mode, upstream_api_base,
                    provider_api_key, model, ocr_cloud_mode, remaining_uses,
                    redeemed_uses, enabled, expires_at, created_at, updated_at
                ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 0, 1, ?, ?, ?)
                """,
                (
                    digest(normalized),
                    normalized[-4:],
                    label.strip(),
                    mode,
                    clean_api_base,
                    clean_key,
                    clean_model,
                    ocr_cloud_mode,
                    uses,
                    expiry,
                    now,
                    now,
                ),
            )
            code_id = int(cursor.lastrowid)
    except sqlite3.IntegrityError as exc:
        raise AdminValidationError("该兑换码已经存在，请换一个兑换码。") from exc
    return code_id, visible_code


def list_redemption_codes(store: GatewayStore) -> list[dict[str, Any]]:
    with store.connect() as conn:
        rows = conn.execute(
            """
            SELECT
                c.id, c.code_hint, c.label, c.mode, c.upstream_api_base, c.model,
                c.ocr_cloud_mode, c.remaining_uses, c.redeemed_uses, c.enabled,
                c.expires_at, c.created_at, c.updated_at,
                COUNT(a.id) AS activations,
                COALESCE(SUM(CASE WHEN a.enabled = 1 THEN 1 ELSE 0 END), 0)
                    AS active_activations,
                COALESCE(SUM(a.request_count), 0) AS requests
            FROM redemption_codes c
            LEFT JOIN activations a ON a.code_id = c.id
            GROUP BY c.id
            ORDER BY c.id DESC
            """
        ).fetchall()
    return [dict(row) for row in rows]


def get_redemption_code(store: GatewayStore, code_id: int) -> dict[str, Any] | None:
    with store.connect() as conn:
        row = conn.execute(
            """
            SELECT
                id, code_hint, label, mode, upstream_api_base, model,
                ocr_cloud_mode, remaining_uses, redeemed_uses, enabled,
                expires_at, created_at, updated_at
            FROM redemption_codes
            WHERE id = ?
            """,
            (code_id,),
        ).fetchone()
    return dict(row) if row is not None else None


def resolve_code_id(
    store: GatewayStore,
    *,
    code_id: int | None = None,
    code: str | None = None,
) -> int | None:
    with store.connect() as conn:
        if code_id is not None:
            row = conn.execute(
                "SELECT id FROM redemption_codes WHERE id = ?", (code_id,)
            ).fetchone()
        else:
            normalized = normalize_code(code or "")
            row = conn.execute(
                "SELECT id FROM redemption_codes WHERE code_hash = ?",
                (digest(normalized),),
            ).fetchone()
    return int(row["id"]) if row is not None else None


def update_redemption_code(
    store: GatewayStore,
    code_id: int,
    changes: Mapping[str, object],
) -> None:
    allowed = {
        "label",
        "mode",
        "upstream_api_base",
        "provider_api_key",
        "model",
        "ocr_cloud_mode",
        "remaining_uses",
        "expires_at",
        "enabled",
    }
    if not changes:
        raise AdminValidationError("没有需要保存的修改。")
    if set(changes) - allowed:
        raise AdminValidationError("包含不允许修改的字段。")

    updates: dict[str, object] = {}
    if "label" in changes:
        updates["label"] = str(changes["label"]).strip()
    if "mode" in changes:
        mode = str(changes["mode"])
        if mode not in ALLOWED_MODES:
            raise AdminValidationError("调用模式不正确。")
        updates["mode"] = mode
    if "upstream_api_base" in changes:
        try:
            updates["upstream_api_base"] = validated_base_url(
                str(changes["upstream_api_base"])
            )
        except ValueError as exc:
            raise AdminValidationError(
                "API 地址必须是完整的 http:// 或 https:// 地址。"
            ) from exc
    if "provider_api_key" in changes:
        key = str(changes["provider_api_key"]).strip()
        if not key:
            raise AdminValidationError("新 API Key 不能为空。")
        updates["provider_api_key"] = key
    if "model" in changes:
        model = str(changes["model"]).strip()
        if not model:
            raise AdminValidationError("模型名称不能为空。")
        updates["model"] = model
    if "ocr_cloud_mode" in changes:
        ocr_mode = str(changes["ocr_cloud_mode"])
        if ocr_mode not in ALLOWED_OCR_MODES:
            raise AdminValidationError("OCR 云端模式不正确。")
        updates["ocr_cloud_mode"] = ocr_mode
    if "remaining_uses" in changes:
        updates["remaining_uses"] = _validated_nonnegative_integer(
            changes["remaining_uses"], "剩余次数"
        )
    if "expires_at" in changes:
        updates["expires_at"] = _validated_expiry(changes["expires_at"])
    if "enabled" in changes:
        enabled = changes["enabled"]
        updates["enabled"] = 1 if enabled in (True, 1, "1", "yes", "true", "on") else 0

    updates["updated_at"] = utc_now()
    assignments = ", ".join(f"{column} = ?" for column in updates)
    with store.connect() as conn:
        cursor = conn.execute(
            f"UPDATE redemption_codes SET {assignments} WHERE id = ?",
            (*updates.values(), code_id),
        )
    if cursor.rowcount == 0:
        raise AdminValidationError("兑换码不存在。")


def list_activations(store: GatewayStore, code_id: int) -> list[dict[str, Any]]:
    with store.connect() as conn:
        rows = conn.execute(
            """
            SELECT id, enabled, request_count, created_at, last_redeemed_at, last_used_at
            FROM activations
            WHERE code_id = ?
            ORDER BY id DESC
            """,
            (code_id,),
        ).fetchall()
    return [dict(row) for row in rows]


def set_activation_enabled(
    store: GatewayStore,
    activation_id: int,
    enabled: bool,
) -> int:
    with store.connect() as conn:
        row = conn.execute(
            "SELECT code_id FROM activations WHERE id = ?", (activation_id,)
        ).fetchone()
        if row is None:
            raise AdminValidationError("设备激活记录不存在。")
        conn.execute(
            "UPDATE activations SET enabled = ? WHERE id = ?",
            (1 if enabled else 0, activation_id),
        )
    return int(row["code_id"])
