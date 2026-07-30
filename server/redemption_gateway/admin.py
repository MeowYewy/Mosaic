#!/usr/bin/env python3
"""Local administration CLI for Mosaic redemption codes."""

from __future__ import annotations

import argparse
import getpass
from pathlib import Path

from admin_service import (
    AdminValidationError,
    create_redemption_code,
    get_redemption_code,
    list_activations as service_list_activations,
    list_redemption_codes,
    resolve_code_id,
    set_activation_enabled,
    update_redemption_code,
)
from admin_auth import AdminAuthManager
from gateway import GatewayStore


def read_key(confirm: bool = False) -> str:
    value = getpass.getpass("Provider API key: ").strip()
    if not value:
        raise SystemExit("API key cannot be empty.")
    if confirm:
        second = getpass.getpass("Provider API key again: ").strip()
        if value != second:
            raise SystemExit("API keys did not match.")
    return value


def create_code(store: GatewayStore, args: argparse.Namespace) -> None:
    api_key = read_key(confirm=True)
    try:
        _, code = create_redemption_code(
            store,
            code=args.code,
            label=args.label,
            mode=args.mode,
            api_base=args.api_base,
            provider_api_key=api_key,
            model=args.model,
            ocr_cloud_mode=args.ocr_cloud_mode,
            remaining_uses=args.uses,
            expires_at=args.expires_at,
        )
    except AdminValidationError as exc:
        raise SystemExit(str(exc)) from exc
    print(f"Created redemption code: {code}")
    print("Save it now. The database stores only its SHA-256 hash.")


def list_codes(store: GatewayStore, _args: argparse.Namespace) -> None:
    rows = list_redemption_codes(store)
    if not rows:
        print("No redemption codes.")
        return
    headers = (
        "ID",
        "HINT",
        "LABEL",
        "MODE",
        "MODEL",
        "REMAIN",
        "USED",
        "ACTIVE",
        "REQUESTS",
        "ENABLED",
        "EXPIRES",
    )
    print("\t".join(headers))
    for row in rows:
        print(
            "\t".join(
                [
                    str(row["id"]),
                    "…" + row["code_hint"],
                    row["label"] or "-",
                    row["mode"],
                    row["model"],
                    str(row["remaining_uses"]),
                    str(row["redeemed_uses"]),
                    str(row["activations"]),
                    str(row["requests"]),
                    "yes" if row["enabled"] else "no",
                    row["expires_at"] or "-",
                ]
            )
        )


def update_code(store: GatewayStore, args: argparse.Namespace) -> None:
    updates: dict[str, object] = {}
    if args.label is not None:
        updates["label"] = args.label.strip()
    if args.mode is not None:
        updates["mode"] = args.mode
    if args.api_base is not None:
        updates["upstream_api_base"] = args.api_base
    if args.model is not None:
        updates["model"] = args.model
    if args.ocr_cloud_mode is not None:
        updates["ocr_cloud_mode"] = args.ocr_cloud_mode
    if args.remaining_uses is not None:
        updates["remaining_uses"] = args.remaining_uses
    if args.expires_at is not None:
        updates["expires_at"] = args.expires_at
    if args.enabled is not None:
        updates["enabled"] = args.enabled
    if args.change_api_key:
        updates["provider_api_key"] = read_key(confirm=True)
    if not updates:
        raise SystemExit("No changes were requested.")

    code_id = resolve_code_id(store, code_id=args.id, code=args.code)
    if code_id is None:
        raise SystemExit("Redemption code not found.")
    try:
        update_redemption_code(store, code_id, updates)
    except AdminValidationError as exc:
        raise SystemExit(str(exc)) from exc
    row = get_redemption_code(store, code_id)
    if row is None:
        raise SystemExit("Redemption code not found.")
    print(f"Updated redemption code ID {row['id']} (hint …{row['code_hint']}).")


def revoke_activation(store: GatewayStore, args: argparse.Namespace) -> None:
    try:
        set_activation_enabled(store, args.activation_id, False)
    except AdminValidationError as exc:
        raise SystemExit(str(exc)) from exc
    print(f"Revoked activation {args.activation_id}.")


def list_activations(store: GatewayStore, args: argparse.Namespace) -> None:
    code_id = resolve_code_id(store, code_id=args.id, code=args.code)
    if code_id is None:
        raise SystemExit("Redemption code not found.")
    row = get_redemption_code(store, code_id)
    if row is None:
        raise SystemExit("Redemption code not found.")
    activations = service_list_activations(store, code_id)
    print(f"Code ID {row['id']} (hint …{row['code_hint']}):")
    if not activations:
        print("No activations.")
        return
    print("ID\tENABLED\tREQUESTS\tCREATED\tLAST_REDEEMED\tLAST_USED")
    for activation in activations:
        print(
            "\t".join(
                [
                    str(activation["id"]),
                    "yes" if activation["enabled"] else "no",
                    str(activation["request_count"]),
                    activation["created_at"],
                    activation["last_redeemed_at"],
                    activation["last_used_at"] or "-",
                ]
            )
        )


def add_selector(parser: argparse.ArgumentParser) -> None:
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--id", type=int)
    group.add_argument("--code")


def set_admin_password(args: argparse.Namespace) -> None:
    auth = AdminAuthManager(args.credentials)
    default_username = "admin"
    if auth.credentials_configured():
        existing = auth.load_credentials()
        if existing is not None:
            default_username = existing.username
    username = input(f"Admin username [{default_username}]: ").strip() or default_username
    password = getpass.getpass("Admin password: ")
    confirm = getpass.getpass("Admin password again: ")
    if password != confirm:
        raise SystemExit("Passwords did not match.")
    try:
        auth.set_password(username, password)
    except ValueError as exc:
        raise SystemExit(str(exc)) from exc
    print(f"Admin credentials saved: {Path(args.credentials).resolve()}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Manage Mosaic redemption codes")
    parser.add_argument(
        "--database",
        default=str(Path(__file__).resolve().parent / "data" / "gateway.sqlite3"),
    )
    parser.add_argument(
        "--credentials",
        default=str(
            Path(__file__).resolve().parent / "data" / "admin_credentials.json"
        ),
        help="Admin login credentials JSON file",
    )
    commands = parser.add_subparsers(dest="command", required=True)

    commands.add_parser("init", help="Create or upgrade the database")

    commands.add_parser(
        "set-admin-password",
        help="Set or reset the browser admin login password",
    )

    create = commands.add_parser("create", help="Create a redemption code")
    create.add_argument("--code", help="Custom code; omitted means generate one")
    create.add_argument("--label", default="")
    create.add_argument("--mode", choices=("text", "qwen_ocr"), required=True)
    create.add_argument("--api-base", required=True, help="Provider API base URL")
    create.add_argument("--model", required=True)
    create.add_argument("--ocr-cloud-mode", choices=("single", "dual"), default="single")
    create.add_argument("--uses", type=int, required=True)
    create.add_argument("--expires-at", help="ISO-8601 UTC time, for example 2026-12-31T16:00:00Z")

    commands.add_parser("list", help="List codes without revealing keys or full codes")

    update = commands.add_parser("update", help="Update a code")
    add_selector(update)
    update.add_argument("--label")
    update.add_argument("--mode", choices=("text", "qwen_ocr"))
    update.add_argument("--api-base")
    update.add_argument("--model")
    update.add_argument("--ocr-cloud-mode", choices=("single", "dual"))
    update.add_argument("--remaining-uses", type=int)
    update.add_argument("--expires-at", help="ISO-8601 time; pass an empty string to clear")
    update.add_argument("--enabled", choices=("yes", "no"))
    update.add_argument("--change-api-key", action="store_true")

    activations = commands.add_parser("activations", help="List activations for a code")
    add_selector(activations)

    revoke = commands.add_parser("revoke-activation", help="Revoke one activated client")
    revoke.add_argument("--activation-id", type=int, required=True)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    store = GatewayStore(args.database)
    store.initialize()
    if args.command == "init":
        print(f"Database ready: {Path(args.database).resolve()}")
    elif args.command == "set-admin-password":
        set_admin_password(args)
    elif args.command == "create":
        create_code(store, args)
    elif args.command == "list":
        list_codes(store, args)
    elif args.command == "update":
        update_code(store, args)
    elif args.command == "activations":
        list_activations(store, args)
    elif args.command == "revoke-activation":
        revoke_activation(store, args)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
