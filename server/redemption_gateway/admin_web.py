#!/usr/bin/env python3
"""Loopback-only browser administration panel for Mosaic redemption codes."""

from __future__ import annotations

import argparse
import html
import json
import re
import secrets
import threading
import webbrowser
from datetime import datetime
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs, urlencode, urlsplit

from admin_service import (
    AdminValidationError,
    create_redemption_code,
    get_redemption_code,
    list_activations,
    list_redemption_codes,
    set_activation_enabled,
    update_redemption_code,
)
from admin_auth import (
    SESSION_COOKIE,
    AdminAuthManager,
    join_base_path,
    normalize_base_path,
)


from gateway import GatewayStore, parse_expiry


LOGIN_ROUTE = "/login"
LOGOUT_ROUTE = "/logout"
CODE_ROUTE = re.compile(r"^/codes/(\d+)$")
ACTIVATIONS_ROUTE = re.compile(r"^/codes/(\d+)/activations$")
ACTIVATION_TOGGLE_ROUTE = re.compile(r"^/activations/(\d+)/toggle$")
MAX_FORM_BYTES = 256 * 1024


def escape(value: object) -> str:
    return html.escape(str(value if value is not None else ""), quote=True)


def selected(value: object, expected: str) -> str:
    return " selected" if str(value) == expected else ""


def expiry_input_value(value: object) -> str:
    if not value:
        return ""
    try:
        parsed = parse_expiry(str(value))
    except (TypeError, ValueError):
        return ""
    return parsed.strftime("%Y-%m-%dT%H:%M") if parsed else ""


def page_shell(
    title: str,
    content: str,
    *,
    base_path: str = "",
    csrf_token: str = "",
    show_logout: bool = False,
) -> str:
    prefix = normalize_base_path(base_path)
    codes_url = escape(join_base_path(prefix, "/codes"))
    new_url = escape(join_base_path(prefix, "/codes/new"))
    logout_html = ""
    if show_logout:
        logout_html = f"""
        <form method="post" action="{escape(join_base_path(prefix, LOGOUT_ROUTE))}" style="display:inline">
          <input type="hidden" name="csrf_token" value="{escape(csrf_token)}">
          <button class="button secondary" type="submit" style="min-height:34px">退出</button>
        </form>"""
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escape(title)} · Mosaic 管理后台</title>
  <style>
    :root {{
      color-scheme: light;
      --ink: #173b39;
      --muted: #627b79;
      --line: #dbe8e6;
      --paper: #ffffff;
      --wash: #f3f8f7;
      --brand: #0f766e;
      --brand-dark: #115e59;
      --danger: #b42318;
      --danger-wash: #fff1f0;
      --ok: #067647;
      --ok-wash: #ecfdf3;
    }}
    * {{ box-sizing: border-box; }}
    body {{
      margin: 0;
      background: var(--wash);
      color: var(--ink);
      font: 15px/1.55 "Segoe UI", "Microsoft YaHei", sans-serif;
    }}
    header {{
      background: linear-gradient(120deg, #134e4a, #0f766e);
      color: white;
      box-shadow: 0 8px 24px rgba(15, 118, 110, .18);
    }}
    .nav {{
      width: min(1180px, calc(100% - 32px));
      margin: auto;
      min-height: 68px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 16px;
    }}
    .brand {{ font-size: 18px; font-weight: 700; letter-spacing: .02em; }}
    .nav a {{ color: white; text-decoration: none; }}
    .nav-links {{ display: flex; gap: 10px; }}
    .nav-links a {{
      border: 1px solid rgba(255,255,255,.35);
      border-radius: 9px;
      padding: 7px 12px;
    }}
    main {{
      width: min(1180px, calc(100% - 32px));
      margin: 30px auto 64px;
    }}
    h1 {{ margin: 0 0 8px; font-size: clamp(25px, 4vw, 34px); }}
    h2 {{ margin: 0 0 18px; font-size: 20px; }}
    p {{ margin: 8px 0; }}
    .muted {{ color: var(--muted); }}
    .toolbar {{
      display: flex;
      align-items: flex-start;
      justify-content: space-between;
      gap: 18px;
      margin-bottom: 22px;
    }}
    .card {{
      background: var(--paper);
      border: 1px solid var(--line);
      border-radius: 14px;
      box-shadow: 0 8px 26px rgba(23, 59, 57, .06);
      padding: 22px;
    }}
    .table-wrap {{
      overflow-x: auto;
      background: var(--paper);
      border: 1px solid var(--line);
      border-radius: 14px;
      box-shadow: 0 8px 26px rgba(23, 59, 57, .06);
    }}
    table {{ width: 100%; border-collapse: collapse; min-width: 920px; }}
    th, td {{
      padding: 13px 15px;
      text-align: left;
      border-bottom: 1px solid var(--line);
      vertical-align: middle;
    }}
    th {{
      color: var(--muted);
      background: #f8fbfa;
      font-size: 13px;
      white-space: nowrap;
    }}
    tr:last-child td {{ border-bottom: 0; }}
    .badge {{
      display: inline-block;
      border-radius: 999px;
      padding: 3px 9px;
      background: #e6f4f1;
      color: var(--brand-dark);
      font-size: 12px;
      font-weight: 650;
      white-space: nowrap;
    }}
    .badge.off {{ background: #f2f4f7; color: #667085; }}
    .button {{
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-height: 38px;
      border: 0;
      border-radius: 9px;
      padding: 8px 14px;
      background: var(--brand);
      color: white;
      cursor: pointer;
      font: inherit;
      font-weight: 650;
      text-decoration: none;
      white-space: nowrap;
    }}
    .button:hover {{ background: var(--brand-dark); }}
    .button.secondary {{
      background: white;
      color: var(--brand-dark);
      border: 1px solid #9dc9c4;
    }}
    .button.danger {{
      background: white;
      color: var(--danger);
      border: 1px solid #f0b7b2;
    }}
    .actions {{ display: flex; flex-wrap: wrap; gap: 8px; }}
    form {{ margin: 0; }}
    .form-grid {{
      display: grid;
      grid-template-columns: repeat(2, minmax(0, 1fr));
      gap: 18px;
    }}
    .field.full {{ grid-column: 1 / -1; }}
    label {{ display: block; margin-bottom: 6px; font-weight: 650; }}
    input, select {{
      width: 100%;
      min-height: 43px;
      border: 1px solid #b8cfcc;
      border-radius: 9px;
      padding: 8px 11px;
      background: white;
      color: var(--ink);
      font: inherit;
      outline: none;
    }}
    input:focus, select:focus {{
      border-color: var(--brand);
      box-shadow: 0 0 0 3px rgba(15,118,110,.12);
    }}
    input[type="checkbox"] {{ width: 18px; min-height: 18px; }}
    .check-line {{ display: flex; align-items: center; gap: 9px; min-height: 43px; }}
    .check-line label {{ margin: 0; }}
    .hint {{ margin-top: 5px; color: var(--muted); font-size: 13px; }}
    .form-footer {{
      display: flex;
      gap: 10px;
      justify-content: flex-end;
      margin-top: 24px;
      padding-top: 20px;
      border-top: 1px solid var(--line);
    }}
    .notice {{
      border-radius: 10px;
      padding: 12px 14px;
      margin: 0 0 20px;
      border: 1px solid;
    }}
    .notice.ok {{ background: var(--ok-wash); border-color: #abefc6; color: var(--ok); }}
    .notice.error {{
      background: var(--danger-wash);
      border-color: #f6c2be;
      color: var(--danger);
    }}
    .empty {{ text-align: center; padding: 52px 24px; }}
    .code-box {{
      margin: 22px 0;
      border: 1px dashed #72b7b0;
      border-radius: 12px;
      background: #effaf8;
      padding: 22px;
      text-align: center;
    }}
    .code-value {{
      display: block;
      margin: 5px 0 14px;
      font: 700 clamp(20px, 4vw, 30px)/1.3 Consolas, monospace;
      letter-spacing: .05em;
      overflow-wrap: anywhere;
    }}
    .split {{
      display: grid;
      grid-template-columns: minmax(0, 2fr) minmax(260px, 1fr);
      gap: 20px;
      align-items: start;
    }}
    .summary {{ display: grid; gap: 10px; }}
    .summary-item {{
      display: flex;
      justify-content: space-between;
      gap: 14px;
      border-bottom: 1px solid var(--line);
      padding-bottom: 9px;
    }}
    .summary-item:last-child {{ border: 0; padding-bottom: 0; }}
    @media (max-width: 760px) {{
      .toolbar, .form-footer {{ align-items: stretch; flex-direction: column; }}
      .form-grid, .split {{ grid-template-columns: 1fr; }}
      .field.full {{ grid-column: auto; }}
      .button {{ width: 100%; }}
    }}
  </style>
</head>
<body>
  <header>
    <nav class="nav">
      <a class="brand" href="{codes_url}">Mosaic 兑换码后台</a>
      <div class="nav-links">
        <a href="{codes_url}">兑换码</a>
        <a href="{new_url}">新建</a>
        {logout_html}
      </div>
    </nav>
  </header>
  <main>{content}</main>
</body>
</html>"""


def notice(message: str, kind: str = "ok") -> str:
    return f'<div class="notice {escape(kind)}">{escape(message)}</div>'


def login_page(
    *,
    base_path: str = "",
    csrf_token: str,
    error: str = "",
    configured: bool = True,
    next_path: str = "",
) -> str:
    prefix = normalize_base_path(base_path)
    action = escape(join_base_path(prefix, LOGIN_ROUTE))
    error_html = notice(error, "error") if error else ""
    if not configured:
        body = f"""
        <div class="card empty">
          <h1>尚未设置管理密码</h1>
          {notice("请先在服务器上运行：python admin.py set-admin-password", "error")}
        </div>"""
    else:
        next_field = ""
        if next_path.startswith("/"):
            next_field = f'<input type="hidden" name="next" value="{escape(next_path)}">'
        body = f"""
        <div class="card" style="max-width:420px;margin:48px auto">
          <h1>管理登录</h1>
          <p class="muted">登录后可创建和管理兑换码。</p>
          {error_html}
          <form method="post" action="{action}" autocomplete="off">
            <input type="hidden" name="csrf_token" value="{escape(csrf_token)}">
            {next_field}
            <div class="field">
              <label for="username">用户名</label>
              <input id="username" name="username" required autocomplete="username">
            </div>
            <div class="field">
              <label for="password">密码</label>
              <input id="password" name="password" type="password" required
                     autocomplete="current-password">
            </div>
            <div class="form-footer">
              <button class="button" type="submit">登录</button>
            </div>
          </form>
        </div>"""
    return page_shell("登录", body, base_path=base_path, csrf_token=csrf_token)


def code_form(
    csrf_token: str,
    *,
    base_path: str = "",
    row: dict[str, Any] | None = None,
    error: str = "",
    values: dict[str, str] | None = None,
) -> str:
    editing = row is not None
    source: dict[str, Any] = dict(row or {})
    if values:
        source.update(values)
        if editing:
            source["enabled"] = values.get("enabled") == "1"
    mode = str(source.get("mode", "qwen_ocr"))
    api_base = str(
        source.get(
            "api_base",
            source.get(
                "upstream_api_base",
                "https://dashscope.aliyuncs.com/api/v1",
            ),
        )
    )
    model = str(source.get("model", "qwen3.5-ocr"))
    ocr_mode = str(source.get("ocr_cloud_mode", "single"))
    prefix = normalize_base_path(base_path)
    action = escape(join_base_path(prefix, f"/codes/{row['id']}" if editing else "/codes"))
    codes_url = escape(join_base_path(prefix, "/codes"))
    heading = f"编辑兑换码 · …{escape(row['code_hint'])}" if editing else "创建兑换码"
    error_html = notice(error, "error") if error else ""
    code_field = ""
    if not editing:
        code_field = f"""
        <div class="field">
          <label for="code">自定义兑换码（可选）</label>
          <input id="code" name="code" maxlength="80"
                 value="{escape(source.get('code', ''))}"
                 placeholder="留空则自动生成">
          <div class="hint">自动生成的兑换码只会完整显示一次。</div>
        </div>"""
    enabled_field = ""
    if editing:
        is_enabled = bool(int(source.get("enabled", 0)))
        enabled_field = f"""
        <div class="field">
          <label>兑换码状态</label>
          <div class="check-line">
            <input type="checkbox" id="enabled" name="enabled" value="1"
                   {"checked" if is_enabled else ""}>
            <label for="enabled">允许兑换及继续调用</label>
          </div>
        </div>"""
    key_label = "更换 API Key（不修改请留空）" if editing else "API Key"
    key_required = "" if editing else " required"
    key_hint = (
        "后台不会显示现有 Key；填写后会立即替换，已经激活的设备自动使用新 Key。"
        if editing
        else "真实 Key 仅写入服务器数据库，不会下发给 Mosaic 客户端。"
    )
    submit_label = "保存修改" if editing else "创建并显示兑换码"
    return page_shell(
        heading,
        f"""
        <div class="toolbar">
          <div>
            <h1>{heading}</h1>
            <p class="muted">填写模型配置和允许首次激活的设备数量。</p>
          </div>
          <a class="button secondary" href="{codes_url}">返回列表</a>
        </div>
        {error_html}
        <form class="card" method="post" action="{action}" autocomplete="off">
          <input type="hidden" name="csrf_token" value="{escape(csrf_token)}">
          <div class="form-grid">
            <div class="field">
              <label for="label">备注名称</label>
              <input id="label" name="label" maxlength="120"
                     value="{escape(source.get('label', ''))}"
                     placeholder="例如：A客户 10 台设备">
            </div>
            {code_field}
            <div class="field">
              <label for="mode">用途</label>
              <select id="mode" name="mode" required>
                <option value="qwen_ocr"{selected(mode, "qwen_ocr")}>千问 OCR</option>
                <option value="text"{selected(mode, "text")}>文本模型</option>
              </select>
            </div>
            <div class="field">
              <label for="model">模型名称</label>
              <input id="model" name="model" required
                     value="{escape(model)}" placeholder="qwen3.5-ocr">
            </div>
            <div class="field full">
              <label for="api_base">模型服务 API 地址</label>
              <input id="api_base" name="api_base" type="url" required
                     value="{escape(api_base)}"
                     placeholder="https://dashscope.aliyuncs.com/api/v1">
            </div>
            <div class="field">
              <label for="ocr_cloud_mode">OCR 云端模式</label>
              <select id="ocr_cloud_mode" name="ocr_cloud_mode">
                <option value="single"{selected(ocr_mode, "single")}>单页/单图</option>
                <option value="dual"{selected(ocr_mode, "dual")}>双图模式</option>
              </select>
            </div>
            <div class="field">
              <label for="remaining_uses">剩余首次激活次数</label>
              <input id="remaining_uses" name="remaining_uses" type="number"
                     min="0" step="1" required
                     value="{escape(source.get('remaining_uses', 1))}">
              <div class="hint">同一设备重复兑换不会再次扣次数。</div>
            </div>
            <div class="field">
              <label for="expires_at">到期时间（可选，UTC）</label>
              <input id="expires_at" name="expires_at" type="datetime-local"
                     value="{escape(source.get('expires_at_input', expiry_input_value(source.get('expires_at'))))}">
            </div>
            {enabled_field}
            <div class="field">
              <label for="provider_api_key">{escape(key_label)}</label>
              <input id="provider_api_key" name="provider_api_key" type="password"
                     value=""{key_required} autocomplete="new-password">
              <div class="hint">{escape(key_hint)}</div>
            </div>
            <div class="field">
              <label for="provider_api_key_confirm">再次输入 API Key</label>
              <input id="provider_api_key_confirm" name="provider_api_key_confirm"
                     type="password" value=""{key_required} autocomplete="new-password">
            </div>
          </div>
          <div class="form-footer">
            <a class="button secondary" href="{codes_url}">取消</a>
            <button class="button" type="submit">{escape(submit_label)}</button>
          </div>
        </form>
        <script>
          const mode = document.getElementById("mode");
          const apiBase = document.getElementById("api_base");
          const model = document.getElementById("model");
          const ocrMode = document.getElementById("ocr_cloud_mode");
          mode.addEventListener("change", () => {{
            if (mode.value === "qwen_ocr") {{
              apiBase.value = "https://dashscope.aliyuncs.com/api/v1";
              model.value = "qwen3.5-ocr";
              ocrMode.disabled = false;
            }} else {{
              apiBase.value = "https://api.moonshot.cn/v1";
              model.value = "moonshot-v1-8k";
              ocrMode.value = "single";
              ocrMode.disabled = true;
            }}
          }});
          ocrMode.disabled = mode.value !== "qwen_ocr";
        </script>
        """,
        base_path=base_path,
        csrf_token=csrf_token,
        show_logout=True,
    )


class AdminApplication:
    def __init__(
        self,
        store: GatewayStore,
        auth: AdminAuthManager,
        *,
        base_path: str = "",
    ) -> None:
        self.store = store
        self.auth = auth
        self.base_path = normalize_base_path(base_path)
        self.csrf_token = secrets.token_urlsafe(32)

    def url(self, path: str) -> str:
        return join_base_path(self.base_path, path)

    def codes_page(self, query: dict[str, list[str]]) -> str:
        rows = list_redemption_codes(self.store)
        message_html = ""
        if query.get("saved") == ["1"]:
            message_html = notice("兑换码配置已保存。")
        if query.get("activation") == ["updated"]:
            message_html = notice("设备状态已更新。")
        row_html = []
        for row in rows:
            status = (
                '<span class="badge">启用</span>'
                if row["enabled"]
                else '<span class="badge off">停用</span>'
            )
            purpose = "千问 OCR" if row["mode"] == "qwen_ocr" else "文本"
            row_html.append(
                f"""
                <tr>
                  <td>#{row['id']}</td>
                  <td><strong>{escape(row['label'] or '未命名')}</strong><br>
                      <span class="muted">尾号 …{escape(row['code_hint'])}</span></td>
                  <td><span class="badge">{purpose}</span><br>
                      <span class="muted">{escape(row['model'])}</span></td>
                  <td><strong>{row['remaining_uses']}</strong> 次<br>
                      <span class="muted">已兑换 {row['redeemed_uses']}</span></td>
                  <td>{row['active_activations']} / {row['activations']}<br>
                      <span class="muted">{row['requests']} 次请求</span></td>
                  <td>{status}</td>
                  <td>
                    <div class="actions">
                      <a class="button secondary"
                         href="{escape(self.url(f"/codes/{row['id']}"))}">编辑</a>
                      <a class="button secondary"
                         href="{escape(self.url(f"/codes/{row['id']}/activations"))}">设备</a>
                    </div>
                  </td>
                </tr>
                """
            )
        if row_html:
            content = f"""
            <div class="table-wrap">
              <table>
                <thead>
                  <tr>
                    <th>ID</th><th>名称 / 兑换码</th><th>用途 / 模型</th>
                    <th>剩余次数</th><th>设备 / 请求</th><th>状态</th><th>操作</th>
                  </tr>
                </thead>
                <tbody>{''.join(row_html)}</tbody>
              </table>
            </div>"""
        else:
            content = f"""
            <div class="card empty">
              <h2>还没有兑换码</h2>
              <p class="muted">创建第一个兑换码，配置模型、Key 和可激活设备数。</p>
              <p><a class="button" href="{escape(self.url("/codes/new"))}">创建兑换码</a></p>
            </div>"""
        return page_shell(
            "兑换码",
            f"""
            <div class="toolbar">
              <div>
                <h1>兑换码</h1>
                <p class="muted">真实 API Key 始终保存在服务器，不会显示在此页面。</p>
              </div>
              <a class="button" href="{escape(self.url("/codes/new"))}">＋ 创建兑换码</a>
            </div>
            {message_html}
            {content}
            """,
            base_path=self.base_path,
            csrf_token=self.csrf_token,
            show_logout=True,
        )

    def activation_page(self, code_id: int) -> str:
        row = get_redemption_code(self.store, code_id)
        if row is None:
            raise AdminValidationError("兑换码不存在。")
        activations = list_activations(self.store, code_id)
        items: list[str] = []
        for activation in activations:
            currently_enabled = bool(activation["enabled"])
            next_value = "0" if currently_enabled else "1"
            button_label = "撤销设备" if currently_enabled else "恢复设备"
            button_class = "button danger" if currently_enabled else "button secondary"
            status = (
                '<span class="badge">可用</span>'
                if currently_enabled
                else '<span class="badge off">已撤销</span>'
            )
            items.append(
                f"""
                <tr>
                  <td>#{activation['id']}</td>
                  <td>{status}</td>
                  <td>{activation['request_count']}</td>
                  <td>{escape(activation['created_at'])}</td>
                  <td>{escape(activation['last_redeemed_at'])}</td>
                  <td>{escape(activation['last_used_at'] or '尚未调用')}</td>
                  <td>
                    <form method="post"
                          action="{escape(self.url(f"/activations/{activation['id']}/toggle"))}">
                      <input type="hidden" name="csrf_token"
                             value="{escape(self.csrf_token)}">
                      <input type="hidden" name="enabled" value="{next_value}">
                      <button class="{button_class}" type="submit">{button_label}</button>
                    </form>
                  </td>
                </tr>"""
            )
        if items:
            table = f"""
            <div class="table-wrap">
              <table>
                <thead>
                  <tr>
                    <th>设备记录</th><th>状态</th><th>请求数</th>
                    <th>首次激活</th><th>最近兑换</th><th>最近调用</th><th>操作</th>
                  </tr>
                </thead>
                <tbody>{''.join(items)}</tbody>
              </table>
            </div>"""
        else:
            table = """
            <div class="card empty">
              <h2>还没有设备激活</h2>
              <p class="muted">用户首次输入该兑换码后，设备会显示在这里。</p>
            </div>"""
        return page_shell(
            "设备管理",
            f"""
            <div class="toolbar">
              <div>
                <h1>设备管理</h1>
                <p class="muted">{escape(row['label'] or '未命名')} ·
                   兑换码尾号 …{escape(row['code_hint'])}</p>
              </div>
              <div class="actions">
                <a class="button secondary"
                   href="{escape(self.url(f"/codes/{code_id}"))}">编辑兑换码</a>
                <a class="button secondary" href="{escape(self.url("/codes"))}">返回列表</a>
              </div>
            </div>
            {table}
            """,
            base_path=self.base_path,
            csrf_token=self.csrf_token,
            show_logout=True,
        )


class AdminHTTPServer(ThreadingHTTPServer):
    daemon_threads = True

    def __init__(self, server_address: tuple[str, int], app: AdminApplication) -> None:
        self.app = app
        super().__init__(server_address, AdminHandler)


class AdminHandler(BaseHTTPRequestHandler):
    server: AdminHTTPServer

    def log_message(self, format_string: str, *args: object) -> None:
        print(
            f"{self.log_date_time_string()} {self.client_address[0]} "
            + format_string % args,
            flush=True,
        )

    def _send_html(self, status: int, body: str) -> None:
        self._send_bytes(status, body.encode("utf-8"), "text/html; charset=utf-8")

    def _redirect(self, location: str) -> None:
        parsed = urlsplit(location)
        path = parsed.path
        if path.startswith("/"):
            path = join_base_path(self.server.app.base_path, path)
        if parsed.query:
            location = f"{path}?{parsed.query}"
        else:
            location = path
        self._send_bytes(HTTPStatus.SEE_OTHER, b"", "text/plain; charset=utf-8", location=location)

    def _request_path(self) -> str:
        parsed = urlsplit(self.path)
        path = parsed.path.rstrip("/") or "/"
        prefix = self.server.app.base_path
        if prefix and (path == prefix or path.startswith(prefix + "/")):
            path = path[len(prefix) :] or "/"
        return path

    def _client_ip(self) -> str:
        forwarded = self.headers.get("X-Forwarded-For", "").split(",")[0].strip()
        if forwarded:
            return forwarded
        real_ip = self.headers.get("X-Real-IP", "").strip()
        if real_ip:
            return real_ip
        return self.client_address[0]

    def _is_secure(self) -> bool:
        forwarded = self.headers.get("X-Forwarded-Proto", "").split(",")[0].strip().lower()
        return forwarded == "https"

    def _session_token(self) -> str:
        cookie_header = self.headers.get("Cookie", "")
        prefix = SESSION_COOKIE + "="
        for chunk in cookie_header.split(";"):
            chunk = chunk.strip()
            if chunk.startswith(prefix):
                return chunk[len(prefix) :].strip()
        return ""

    def _cookie_path(self) -> str:
        return self.server.app.base_path or "/"

    def _set_session_cookie(self, token: str) -> None:
        parts = [
            f"{SESSION_COOKIE}={token}",
            f"Path={self._cookie_path()}",
            "HttpOnly",
            "SameSite=Lax",
            f"Max-Age={12 * 60 * 60}",
        ]
        if self._is_secure():
            parts.append("Secure")
        self._session_cookie_header = "; ".join(parts)

    def _clear_session_cookie(self) -> None:
        parts = [
            f"{SESSION_COOKIE}=",
            f"Path={self._cookie_path()}",
            "HttpOnly",
            "SameSite=Lax",
            "Max-Age=0",
        ]
        if self._is_secure():
            parts.append("Secure")
        self._session_cookie_header = "; ".join(parts)

    def _send_bytes(
        self,
        status: int,
        body: bytes,
        content_type: str,
        *,
        location: str | None = None,
    ) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("X-Frame-Options", "DENY")
        self.send_header(
            "Content-Security-Policy",
            "default-src 'none'; style-src 'unsafe-inline'; "
            "script-src 'unsafe-inline'; img-src 'self'; "
            "form-action 'self'; base-uri 'none'; frame-ancestors 'none'",
        )
        if location is not None:
            self.send_header("Location", location)
        cookie_header = getattr(self, "_session_cookie_header", None)
        if cookie_header:
            self.send_header("Set-Cookie", cookie_header)
            delattr(self, "_session_cookie_header")
        self.end_headers()
        if body:
            self.wfile.write(body)

    def _authenticated(self) -> bool:
        return self.server.app.auth.session_valid(self._session_token())

    def _require_auth(self) -> bool:
        if self._authenticated():
            return True
        next_path = urlsplit(self.path).path
        if self.server.app.base_path and next_path.startswith(self.server.app.base_path):
            next_path = next_path[len(self.server.app.base_path) :] or "/"
        target = LOGIN_ROUTE
        if next_path not in {LOGIN_ROUTE, "/"}:
            target = LOGIN_ROUTE + "?" + urlencode({"next": next_path})
        self._redirect(target)
        return False

    def _login_page(self, *, error: str = "", next_path: str = "") -> None:
        self._send_html(
            HTTPStatus.OK,
            login_page(
                base_path=self.server.app.base_path,
                csrf_token=self.server.app.csrf_token,
                error=error,
                configured=self.server.app.auth.credentials_configured(),
                next_path=next_path,
            ),
        )

    def _handle_login_post(self, form: dict[str, str]) -> None:
        app = self.server.app
        if not app.auth.credentials_configured():
            self._login_page(error="请先在服务器运行 python admin.py set-admin-password。")
            return
        if not app.auth.login_allowed(self._client_ip()):
            self._login_page(error="登录尝试过于频繁，请稍后再试。")
            return
        username = form.get("username", "").strip()
        password = form.get("password", "")
        if not app.auth.authenticate(username, password):
            self._login_page(error="用户名或密码不正确。")
            return
        token = app.auth.create_session()
        self._set_session_cookie(token)
        next_path = form.get("next", "").strip() or "/codes"
        if not next_path.startswith("/"):
            next_path = "/codes"
        self._redirect(next_path)

    def _handle_logout_post(self) -> None:
        self.server.app.auth.destroy_session(self._session_token())
        self._clear_session_cookie()
        self._redirect(LOGIN_ROUTE)

    def _form(self) -> dict[str, str]:
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError as exc:
            raise AdminValidationError("请求长度不正确。") from exc
        if length <= 0 or length > MAX_FORM_BYTES:
            raise AdminValidationError("表单内容为空或过大。")
        content_type = self.headers.get("Content-Type", "")
        if not content_type.startswith("application/x-www-form-urlencoded"):
            raise AdminValidationError("表单类型不正确。")
        parsed = parse_qs(
            self.rfile.read(length).decode("utf-8"),
            keep_blank_values=True,
            max_num_fields=40,
        )
        return {key: values[-1] for key, values in parsed.items()}

    def _validated_form(self) -> dict[str, str]:
        form = self._form()
        token = form.get("csrf_token", "")
        if not secrets.compare_digest(token, self.server.app.csrf_token):
            raise AdminValidationError("页面已失效，请刷新后重试。")
        return form

    def _error_page(self, status: int, message: str) -> None:
        app = self.server.app
        self._send_html(
            status,
            page_shell(
                "无法完成操作",
                f"""
                <div class="card empty">
                  <h1>无法完成操作</h1>
                  {notice(message, "error")}
                  <p><a class="button" href="{escape(app.url("/codes"))}">返回兑换码列表</a></p>
                </div>
                """,
                base_path=app.base_path,
                csrf_token=app.csrf_token,
                show_logout=self._authenticated(),
            ),
        )

    def do_GET(self) -> None:
        path = self._request_path()
        query = parse_qs(urlsplit(self.path).query)
        try:
            if path == "/healthz":
                body = json.dumps({"ok": True}, separators=(",", ":")).encode("utf-8")
                self._send_bytes(HTTPStatus.OK, body, "application/json; charset=utf-8")
                return
            if path == LOGIN_ROUTE:
                if self._authenticated():
                    self._redirect("/codes")
                    return
                next_path = query.get("next", [""])[0]
                if not next_path.startswith("/"):
                    next_path = ""
                self._login_page(next_path=next_path)
                return
            if path == "/":
                self._redirect("/codes")
                return
            if not self._require_auth():
                return
            if path == "/codes":
                self._send_html(HTTPStatus.OK, self.server.app.codes_page(query))
                return
            if path == "/codes/new":
                app = self.server.app
                self._send_html(
                    HTTPStatus.OK,
                    code_form(
                        app.csrf_token,
                        base_path=app.base_path,
                    ),
                )
                return
            match = CODE_ROUTE.fullmatch(path)
            if match:
                code_id = int(match.group(1))
                row = get_redemption_code(self.server.app.store, code_id)
                if row is None:
                    self._error_page(HTTPStatus.NOT_FOUND, "兑换码不存在。")
                    return
                app = self.server.app
                self._send_html(
                    HTTPStatus.OK,
                    code_form(
                        app.csrf_token,
                        base_path=app.base_path,
                        row=row,
                    ),
                )
                return
            match = ACTIVATIONS_ROUTE.fullmatch(path)
            if match:
                self._send_html(
                    HTTPStatus.OK,
                    self.server.app.activation_page(int(match.group(1))),
                )
                return
            self._error_page(HTTPStatus.NOT_FOUND, "页面不存在。")
        except AdminValidationError as exc:
            self._error_page(HTTPStatus.BAD_REQUEST, str(exc))
        except Exception:
            self._error_page(HTTPStatus.INTERNAL_SERVER_ERROR, "服务器处理失败，请查看终端日志。")
            raise

    def do_POST(self) -> None:
        path = self._request_path()
        try:
            if path == LOGIN_ROUTE:
                self._handle_login_post(self._validated_form())
                return
            if not self._require_auth():
                return
            form = self._validated_form()
            if path == LOGOUT_ROUTE:
                self._handle_logout_post()
                return
            if path == "/codes":
                self._create_code(form)
                return
            match = CODE_ROUTE.fullmatch(path)
            if match:
                self._update_code(int(match.group(1)), form)
                return
            match = ACTIVATION_TOGGLE_ROUTE.fullmatch(path)
            if match:
                self._toggle_activation(int(match.group(1)), form)
                return
            self._error_page(HTTPStatus.NOT_FOUND, "页面不存在。")
        except AdminValidationError as exc:
            self._error_page(HTTPStatus.BAD_REQUEST, str(exc))
        except Exception:
            self._error_page(HTTPStatus.INTERNAL_SERVER_ERROR, "服务器处理失败，请查看终端日志。")
            raise

    def _create_code(self, form: dict[str, str]) -> None:
        app = self.server.app
        key = form.get("provider_api_key", "")
        if key != form.get("provider_api_key_confirm", ""):
            self._send_html(
                HTTPStatus.BAD_REQUEST,
                code_form(
                    app.csrf_token,
                    base_path=app.base_path,
                    error="两次输入的 API Key 不一致。",
                    values=form,
                ),
            )
            return
        try:
            code_id, visible_code = create_redemption_code(
                app.store,
                code=form.get("code"),
                label=form.get("label", ""),
                mode=form.get("mode", ""),
                api_base=form.get("api_base", ""),
                provider_api_key=key,
                model=form.get("model", ""),
                ocr_cloud_mode=form.get("ocr_cloud_mode", "single"),
                remaining_uses=form.get("remaining_uses", ""),
                expires_at=form.get("expires_at"),
            )
        except AdminValidationError as exc:
            self._send_html(
                HTTPStatus.BAD_REQUEST,
                code_form(
                    app.csrf_token,
                    base_path=app.base_path,
                    error=str(exc),
                    values=form,
                ),
            )
            return
        self._send_html(
            HTTPStatus.CREATED,
            page_shell(
                "兑换码创建成功",
                f"""
                <div class="card">
                  <h1>兑换码创建成功</h1>
                  <p class="muted">请立即复制并妥善保存。关闭页面后，后台不能再次显示完整兑换码。</p>
                  <div class="code-box">
                    <span class="muted">完整兑换码</span>
                    <span class="code-value" id="created-code">{escape(visible_code)}</span>
                    <button class="button" id="copy-code" type="button">复制兑换码</button>
                  </div>
                  <div class="form-footer">
                    <a class="button secondary" href="{escape(app.url(f"/codes/{code_id}"))}">编辑配置</a>
                    <a class="button" href="{escape(app.url("/codes"))}">返回兑换码列表</a>
                  </div>
                </div>
                <script>
                  document.getElementById("copy-code").addEventListener("click", async () => {{
                    const value = document.getElementById("created-code").textContent;
                    try {{
                      await navigator.clipboard.writeText(value);
                      document.getElementById("copy-code").textContent = "已复制";
                    }} catch (_) {{
                      window.prompt("请复制兑换码", value);
                    }}
                  }});
                </script>
                """,
                base_path=app.base_path,
                csrf_token=app.csrf_token,
                show_logout=True,
            ),
        )

    def _update_code(self, code_id: int, form: dict[str, str]) -> None:
        app = self.server.app
        row = get_redemption_code(app.store, code_id)
        if row is None:
            self._error_page(HTTPStatus.NOT_FOUND, "兑换码不存在。")
            return
        new_key = form.get("provider_api_key", "")
        if new_key != form.get("provider_api_key_confirm", ""):
            self._send_html(
                HTTPStatus.BAD_REQUEST,
                code_form(
                    app.csrf_token,
                    base_path=app.base_path,
                    row=row,
                    error="两次输入的新 API Key 不一致。",
                    values=form,
                ),
            )
            return
        changes: dict[str, object] = {
            "label": form.get("label", ""),
            "mode": form.get("mode", ""),
            "upstream_api_base": form.get("api_base", ""),
            "model": form.get("model", ""),
            "ocr_cloud_mode": form.get("ocr_cloud_mode", "single"),
            "remaining_uses": form.get("remaining_uses", ""),
            "expires_at": form.get("expires_at", ""),
            "enabled": form.get("enabled") == "1",
        }
        if new_key:
            changes["provider_api_key"] = new_key
        try:
            update_redemption_code(app.store, code_id, changes)
        except AdminValidationError as exc:
            self._send_html(
                HTTPStatus.BAD_REQUEST,
                code_form(
                    app.csrf_token,
                    base_path=app.base_path,
                    row=row,
                    error=str(exc),
                    values=form,
                ),
            )
            return
        self._redirect("/codes?" + urlencode({"saved": "1"}))

    def _toggle_activation(self, activation_id: int, form: dict[str, str]) -> None:
        enabled = form.get("enabled") == "1"
        code_id = set_activation_enabled(
            self.server.app.store,
            activation_id,
            enabled,
        )
        self._redirect(
            f"/codes/{code_id}/activations?"
            + urlencode({"activation": "updated"})
        )


def build_server(
    database: str,
    host: str,
    port: int,
    *,
    credentials_path: str | None = None,
    base_path: str = "",
) -> AdminHTTPServer:
    store = GatewayStore(database)
    store.initialize()
    if credentials_path is None:
        credentials_path = str(
            Path(__file__).resolve().parent / "data" / "admin_credentials.json"
        )
    auth = AdminAuthManager(credentials_path)
    app = AdminApplication(store, auth, base_path=base_path)
    return AdminHTTPServer((host, port), app)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run the local Mosaic browser admin panel")
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
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8788)
    parser.add_argument(
        "--base-path",
        default="",
        help="Public URL prefix when served behind a reverse proxy, e.g. /gateway-admin",
    )
    parser.add_argument("--open-browser", action="store_true")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    if args.host not in {"127.0.0.1", "localhost"}:
        raise SystemExit(
            "For safety, the admin panel only accepts --host 127.0.0.1 or localhost."
        )
    server = build_server(
        args.database,
        args.host,
        args.port,
        credentials_path=args.credentials,
        base_path=args.base_path,
    )
    entry_path = join_base_path(args.base_path, "/login") or "/login"
    url = f"http://127.0.0.1:{server.server_port}{entry_path}"
    print(f"Mosaic admin panel: {url}", flush=True)
    if not server.app.auth.credentials_configured():
        print(
            "Admin password is not set yet. Run: python admin.py set-admin-password",
            flush=True,
        )
    print("Close this window or press Ctrl+C to stop the admin panel.", flush=True)
    if args.open_browser:
        threading.Timer(0.2, lambda: webbrowser.open(url)).start()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
