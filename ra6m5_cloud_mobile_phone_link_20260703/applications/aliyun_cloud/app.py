from __future__ import annotations

import json
import os
import secrets
import sqlite3
import time
from pathlib import Path
from typing import Any

from flask import Flask, abort, jsonify, make_response, redirect, render_template_string, request, url_for


APP_ROOT = Path(__file__).resolve().parent
DATA_DIR = APP_ROOT / "data"
REPORTS_FILE = DATA_DIR / "reports.json"
PATIENTS_FILE = DATA_DIR / "patients.json"
PENDING_RECORD_FILE = DATA_DIR / "pending_record.json"
RUNTIME_STATE_FILE = DATA_DIR / "runtime_state.json"
DB_FILE = DATA_DIR / "tcm_cloud.db"

HOST = os.getenv("APP_HOST", "0.0.0.0")
PORT = int(os.getenv("APP_PORT", "5000"))
BASE_URL = os.getenv("BASE_URL", f"http://127.0.0.1:{PORT}")

app = Flask(__name__)


HOME_TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>中医四诊本地云端联调</title>
  <style>
    :root {
      --bg: #f5efe6;
      --panel: #fffaf3;
      --line: #dfd0bb;
      --ink: #2f271f;
      --muted: #6d6359;
      --accent: #8b4a27;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      color: var(--ink);
      font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
      background: #f7f7f7;
    }
    .wrap {
      width: min(1180px, calc(100% - 28px));
      margin: 24px auto 40px;
    }
    .hero, .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 18px;
      box-shadow: none;
    }
    .hero {
      padding: 24px;
      margin-bottom: 18px;
    }
    .hero h1 {
      margin: 0 0 8px;
      font-size: 30px;
    }
    .hero p {
      margin: 0;
      color: var(--muted);
      line-height: 1.7;
    }
    .grid {
      display: grid;
      grid-template-columns: 1.15fr 0.85fr;
      gap: 18px;
    }
    .panel {
      padding: 20px;
    }
    h2 {
      margin: 0 0 14px;
      font-size: 21px;
    }
    .hint {
      margin-bottom: 12px;
      color: var(--muted);
      line-height: 1.7;
      font-size: 14px;
    }
    .status {
      display: inline-block;
      padding: 6px 10px;
      border-radius: 999px;
      background: #f1e3cf;
      color: #7b4f2b;
      font-size: 13px;
      margin-right: 8px;
      margin-bottom: 8px;
    }
    .card {
      border: 1px solid var(--line);
      border-radius: 14px;
      padding: 14px;
      background: #fff;
      margin-top: 12px;
    }
    .title {
      font-weight: 700;
      margin-bottom: 8px;
    }
    .meta {
      color: var(--muted);
      font-size: 14px;
      line-height: 1.7;
    }
    pre {
      white-space: pre-wrap;
      word-break: break-word;
      background: #fff;
      border: 1px dashed var(--line);
      border-radius: 12px;
      padding: 12px;
      font-size: 13px;
      line-height: 1.6;
      margin: 0;
    }
    .empty {
      color: var(--muted);
      font-size: 14px;
    }
    a {
      color: var(--accent);
      text-decoration: none;
    }
    form {
      display: grid;
      gap: 12px;
    }
    .field span {
      display: block;
      margin-bottom: 6px;
      font-size: 14px;
      color: var(--muted);
    }
    input, select {
      width: 100%;
      padding: 10px 12px;
      border: 1px solid var(--line);
      border-radius: 10px;
      background: #fff;
    }
    button {
      padding: 12px 14px;
      border: 0;
      border-radius: 12px;
      background: var(--accent);
      color: #fff;
      font-size: 15px;
      font-weight: 700;
      cursor: pointer;
    }
    @media (max-width: 920px) {
      .grid { grid-template-columns: 1fr; }
    }
  </style>
</head>
<body>
  <div class="wrap">
    <section class="hero">
      <h1>中医四诊本地云端联调</h1>
      <p>现在你只需要在这个页面里改待上传记录，RA6M5 上电后会先获取这条记录，再通过 ESP32-C3 上传到后端。这样就不用每次改代码重新编译了。</p>
    </section>

    <div class="grid">
      <section class="panel">
        <h2>待上传记录</h2>
        <div class="hint">
          <span class="status">板子拉取接口: GET /api/test-record</span>
          <span class="status">上传接口: POST /api/upload-record</span>
        </div>
        <form method="post" action="{{ url_for('ui_pending_record') }}">
          <div class="field">
            <span>设备编号</span>
            <input type="text" name="device_id" value="{{ pending.device_id }}">
          </div>
          <div class="field">
            <span>手机号</span>
            <input type="text" name="phone" value="{{ pending.phone }}">
          </div>
          <div class="field">
            <span>性别</span>
            <select name="gender">
              <option value="male" {% if pending.gender == "male" %}selected{% endif %}>male</option>
              <option value="female" {% if pending.gender == "female" %}selected{% endif %}>female</option>
            </select>
          </div>
          <div class="field">
            <span>年龄</span>
            <input type="text" name="age" value="{{ pending.age }}">
          </div>
          <div class="field">
            <span>睡眠</span>
            <input type="text" name="sleep" value="{{ pending.sleep }}">
          </div>
          <div class="field">
            <span>疲劳</span>
            <input type="text" name="fatigue" value="{{ pending.fatigue }}">
          </div>
          <div class="field">
            <span>综合倾向</span>
            <input type="text" name="main_tendency" value="{{ pending.main_tendency }}">
          </div>
          <button type="submit">提交并上传</button>
        </form>
        <div class="card">
          <div class="title">当前下发给板子的 JSON</div>
          <pre>{{ pending_json }}</pre>
        </div>
        <div class="card">
          <div class="title">板端拉取状态</div>
          <div class="meta">
            最近一次拉取时间: {{ runtime.last_fetch_time or '还没有记录' }}<br>
            最近一次拉取来源: {{ runtime.last_fetch_remote or '还没有记录' }}<br>
            拉取次数: {{ runtime.fetch_count }}
          </div>
        </div>
      </section>

      <section class="panel">
        <h2>最近上传记录</h2>
        {% if records %}
          {% for record in records %}
          <div class="card">
            <div class="title"><a href="/ui/report/{{ record.record_id }}">{{ record.record_id }}</a></div>
            <div class="meta">
              设备: {{ record.device_id }}<br>
              手机: {{ record.phone }}<br>
              性别: {{ record.gender }} / 年龄: {{ record.age }}<br>
              当前倾向: {{ record.last_summary.main_tendency }}<br>
              上传来源: {{ record.upload_remote or 'unknown' }}
            </div>
          </div>
          {% endfor %}
        {% else %}
          <p class="empty">还没有收到上传记录。</p>
        {% endif %}
      </section>
    </div>
  </div>
</body>
</html>
"""


PORTAL_TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>四诊系统入口</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: #f7f7f7;
      color: #222;
      font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    }
    .wrap {
      width: min(480px, calc(100% - 28px));
      padding: 22px;
      border: 1px solid #ddd;
      border-radius: 14px;
      background: #fff;
    }
    h1 { margin: 0 0 10px; font-size: 24px; }
    p { color: #666; line-height: 1.7; }
    a {
      display: block;
      margin-top: 12px;
      padding: 13px 14px;
      border-radius: 10px;
      background: #8b4a27;
      color: #fff;
      text-decoration: none;
      text-align: center;
      font-weight: 700;
    }
    a.secondary {
      background: #eee;
      color: #222;
    }
  </style>
</head>
<body>
  <main class="wrap">
    <h1>四诊系统入口</h1>
    <p>患者手机端请进入手机号登录；终端调试时再进入信息填写界面。</p>
    <a href="/mobile">手机端：手机号登录查看报告</a>
    <a class="secondary" href="/terminal">终端端：填写并上传检测信息</a>
  </main>
</body>
</html>
"""


REPORT_TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{{ report.record_id }}</title>
  <style>
    body {
      margin: 0;
      background: #f6f0e7;
      color: #2f271f;
      font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    }
    .wrap {
      width: min(920px, calc(100% - 28px));
      margin: 24px auto 40px;
      padding: 24px;
      border-radius: 18px;
      border: 1px solid #dfd0bb;
      background: #fffaf3;
      box-shadow: 0 18px 40px rgba(67, 46, 28, 0.08);
    }
    a {
      color: #8b4a27;
      text-decoration: none;
    }
    h1 {
      margin: 12px 0 8px;
    }
    .meta {
      color: #6d6359;
      line-height: 1.7;
    }
    .block {
      margin-top: 18px;
    }
    .card {
      margin-top: 10px;
      padding: 14px;
      border-radius: 12px;
      border: 1px solid #dfd0bb;
      background: #fff;
    }
    pre {
      white-space: pre-wrap;
      word-break: break-word;
      margin: 0;
      font-size: 13px;
      line-height: 1.6;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <a href="/">返回首页</a>
    <h1>{{ report.record_id }}</h1>
    <div class="meta">
      设备: {{ report.device_id }}<br>
      用户: {{ report.user_id }}<br>
      手机: {{ report.phone }}<br>
      性别: {{ report.gender }} / 年龄: {{ report.age }}<br>
      时间: {{ report.last_summary.time }}
    </div>

    <div class="block">
      <strong>本次结论</strong>
      <div class="card">{{ report.last_summary.main_tendency }}</div>
    </div>

    <div class="block">
      <strong>{{ report["compare_summary"]["title"] }}</strong>
      <div class="card">
        {% for item in report["compare_summary"]["items"] %}
        <div>{{ item }}</div>
        {% endfor %}
      </div>
    </div>

    <div class="block">
      <strong>二维码链接</strong>
      <div class="card"><a href="{{ report.qrcode_url }}">{{ report.qrcode_url }}</a></div>
    </div>

    <div class="block">
      <strong>原始上传数据</strong>
      <div class="card"><pre>{{ report.request_json }}</pre></div>
    </div>
  </div>
</body>
</html>
"""


PATIENT_TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Patient {{ patient.phone }}</title>
  <style>
    body {
      margin: 0;
      background: #f6f0e7;
      color: #2f271f;
      font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    }
    .wrap {
      width: min(920px, calc(100% - 28px));
      margin: 24px auto 40px;
      padding: 24px;
      border-radius: 18px;
      border: 1px solid #dfd0bb;
      background: #fffaf3;
      box-shadow: 0 18px 40px rgba(67, 46, 28, 0.08);
    }
    a { color: #8b4a27; text-decoration: none; }
    .meta { color: #6d6359; line-height: 1.8; }
    .card {
      margin-top: 12px;
      padding: 14px;
      border-radius: 12px;
      border: 1px solid #dfd0bb;
      background: #fff;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <a href="/">返回首页</a>
    <h1>患者档案 {{ patient.phone }}</h1>
    <div class="meta">
      用户ID: {{ patient.user_id }}<br>
      性别/年龄: {{ patient.gender }} / {{ patient.age }}<br>
      首次建档: {{ patient.first_seen }}<br>
      最近更新: {{ patient.last_seen }}<br>
      历史记录数: {{ records|length }}
    </div>
    <h2>历史记录</h2>
    {% for record in records %}
    <div class="card">
      <strong><a href="/ui/report/{{ record.record_id }}">{{ record.record_id }}</a></strong><br>
      时间: {{ record.last_summary.time }}<br>
      结论: {{ record.last_summary.main_tendency }}
    </div>
    {% endfor %}
  </div>
</body>
</html>
"""


MOBILE_LOGIN_TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#8b4a27">
  <link rel="manifest" href="/mobile/manifest.json">
  <title>四诊报告登录</title>
  <style>
    body {
      margin: 0;
      min-height: 100vh;
      display: grid;
      place-items: center;
      background: #f7f7f7;
      color: #2f271f;
      font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    }
    .card {
      width: min(420px, calc(100% - 28px));
      padding: 24px;
      border-radius: 22px;
      background: #fffaf3;
      border: 1px solid #dfd0bb;
      box-shadow: none;
    }
    h1 { margin: 0 0 8px; font-size: 26px; }
    p { color: #6d6359; line-height: 1.7; }
    form { display: grid; gap: 12px; margin-top: 16px; }
    input, button {
      width: 100%;
      padding: 12px 14px;
      border-radius: 12px;
      font-size: 15px;
      box-sizing: border-box;
    }
    input { border: 1px solid #dfd0bb; background: white; }
    button {
      border: 0;
      color: white;
      font-weight: 700;
      background: #8b4a27;
    }
    .hint {
      margin-top: 14px;
      padding: 12px;
      border-radius: 12px;
      background: #f1e3cf;
      color: #7b4f2b;
      line-height: 1.7;
    }
  </style>
</head>
<body>
  <main class="card">
    <h1>四诊报告</h1>
    <p>患者用终端填写手机号并完成检测后，可在这里登录查看本次报告和历史记录。</p>
    <form method="post" action="/mobile/send-code">
      <input name="phone" value="{{ phone or '' }}" placeholder="手机号">
      <button type="submit">获取验证码</button>
    </form>
    <form method="post" action="/mobile/login">
      <input name="phone" value="{{ phone or '' }}" placeholder="手机号">
      <input name="code" placeholder="验证码">
      <button type="submit">登录查看报告</button>
    </form>
    {% if dev_code %}
    <div class="hint">本地演示验证码：<strong>{{ dev_code }}</strong></div>
    {% endif %}
    {% if error %}
    <div class="hint">{{ error }}</div>
    {% endif %}
  </main>
  <script>
    if ("serviceWorker" in navigator) {
      navigator.serviceWorker.register("/mobile/sw.js").catch(function () {});
    }
  </script>
</body>
</html>
"""


MOBILE_RECORDS_TEMPLATE = """
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#8b4a27">
  <link rel="manifest" href="/mobile/manifest.json">
  <title>历史四诊记录</title>
  <style>
    body {
      margin: 0;
      background: #f6f0e7;
      color: #2f271f;
      font-family: "Microsoft YaHei", "PingFang SC", sans-serif;
    }
    .wrap { width: min(780px, calc(100% - 28px)); margin: 22px auto 40px; }
    .hero, .record {
      border: 1px solid #dfd0bb;
      border-radius: 18px;
      background: #fffaf3;
      box-shadow: none;
    }
    .hero { padding: 20px; margin-bottom: 14px; }
    h1 { margin: 0 0 8px; font-size: 26px; }
    .muted { color: #6d6359; line-height: 1.7; }
    .tip {
      margin-top: 12px;
      padding: 12px;
      border-radius: 14px;
      background: #f1e3cf;
      color: #7b4f2b;
      line-height: 1.7;
    }
    .record { padding: 16px; margin-top: 12px; }
    a { color: #8b4a27; text-decoration: none; font-weight: 700; }
    .latest {
      margin-top: 14px;
      padding: 16px;
      border-radius: 16px;
      background: #8b4a27;
      color: white;
    }
    .latest a { color: white; }
    .latest .muted { color: rgba(255,255,255,0.82); }
    .empty {
      padding: 18px;
      border-radius: 16px;
      border: 1px dashed #dfd0bb;
      color: #6d6359;
      background: #fffaf3;
    }
  </style>
</head>
<body>
  <div class="wrap">
    <section class="hero">
      <h1>历史四诊记录</h1>
      <div class="muted">手机号：{{ phone }}，共 {{ records|length }} 条记录。终端上传完成后，记录会自动出现在这里。</div>
      <div class="tip">手机浏览器打开本页后，可以选择“添加到主屏幕”，之后就像 App 一样进入。</div>
    </section>
    {% if records %}
      {% set latest = records[0] %}
      <article class="latest">
        <strong>最新报告</strong>
        <h2><a href="/mobile/report/{{ latest.record_id }}">{{ latest.last_summary.main_tendency }}</a></h2>
        <div class="muted">
          时间：{{ latest.last_summary.time }}<br>
          报告编号：{{ latest.record_id }}
        </div>
      </article>
      {% for record in records %}
      <article class="record">
        <a href="/mobile/report/{{ record.record_id }}">{{ record.record_id }}</a>
        <div class="muted">
          时间：{{ record.last_summary.time }}<br>
          综合倾向：{{ record.last_summary.main_tendency }}<br>
          分享链接：<a href="{{ record.qrcode_url }}">{{ record.qrcode_url }}</a>
        </div>
      </article>
      {% endfor %}
    {% else %}
      <div class="empty">还没有该手机号的四诊记录。板端上传后会自动出现在这里。</div>
    {% endif %}
  </div>
  <script>
    if ("serviceWorker" in navigator) {
      navigator.serviceWorker.register("/mobile/sw.js").catch(function () {});
    }
  </script>
</body>
</html>
"""


def ensure_storage() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    if not REPORTS_FILE.exists():
        REPORTS_FILE.write_text("{}", encoding="utf-8")
    if not PATIENTS_FILE.exists():
        PATIENTS_FILE.write_text("{}", encoding="utf-8")
    if not PENDING_RECORD_FILE.exists():
        save_pending_record(default_pending_record())
    if not RUNTIME_STATE_FILE.exists():
        save_runtime_state(default_runtime_state())
    init_database()


def load_reports() -> dict[str, Any]:
    ensure_storage()
    try:
        data = json.loads(REPORTS_FILE.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else {}
    except json.JSONDecodeError:
        return {}


def save_reports(reports: dict[str, Any]) -> None:
    ensure_storage()
    REPORTS_FILE.write_text(
        json.dumps(reports, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def load_patients() -> dict[str, Any]:
    ensure_storage()
    try:
        data = json.loads(PATIENTS_FILE.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else {}
    except json.JSONDecodeError:
        return {}


def save_patients(patients: dict[str, Any]) -> None:
    ensure_storage()
    PATIENTS_FILE.write_text(
        json.dumps(patients, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def db_connect() -> sqlite3.Connection:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    return conn


def init_database() -> None:
    with db_connect() as conn:
        conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS patients (
                phone TEXT PRIMARY KEY,
                user_id TEXT NOT NULL,
                gender TEXT,
                age INTEGER,
                device_id TEXT,
                first_seen TEXT NOT NULL,
                last_seen TEXT NOT NULL,
                latest_record_id TEXT,
                latest_summary_json TEXT NOT NULL DEFAULT '{}'
            );

            CREATE TABLE IF NOT EXISTS records (
                record_id TEXT PRIMARY KEY,
                phone TEXT NOT NULL,
                user_id TEXT NOT NULL,
                device_id TEXT,
                gender TEXT,
                age INTEGER,
                payload_json TEXT NOT NULL,
                report_json TEXT NOT NULL,
                last_summary_json TEXT NOT NULL,
                compare_summary_json TEXT NOT NULL,
                qrcode_token TEXT NOT NULL UNIQUE,
                qrcode_url TEXT NOT NULL,
                upload_remote TEXT,
                created_at INTEGER NOT NULL,
                FOREIGN KEY(phone) REFERENCES patients(phone)
            );

            CREATE TABLE IF NOT EXISTS verification_codes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                phone TEXT NOT NULL,
                code TEXT NOT NULL,
                expires_at INTEGER NOT NULL,
                used INTEGER NOT NULL DEFAULT 0,
                created_at INTEGER NOT NULL
            );

            CREATE TABLE IF NOT EXISTS qr_tokens (
                token TEXT PRIMARY KEY,
                record_id TEXT NOT NULL,
                phone TEXT NOT NULL,
                expires_at INTEGER,
                created_at INTEGER NOT NULL,
                FOREIGN KEY(record_id) REFERENCES records(record_id)
            );

            CREATE TABLE IF NOT EXISTS sessions (
                token TEXT PRIMARY KEY,
                phone TEXT NOT NULL,
                expires_at INTEGER NOT NULL,
                created_at INTEGER NOT NULL
            );
            """
        )


def build_qrcode_token() -> str:
    return secrets.token_urlsafe(12)


def build_mobile_session_token() -> str:
    return secrets.token_urlsafe(24)


def upsert_patient_db(patient: dict[str, Any]) -> None:
    init_database()
    with db_connect() as conn:
        conn.execute(
            """
            INSERT INTO patients (
                phone, user_id, gender, age, device_id, first_seen, last_seen,
                latest_record_id, latest_summary_json
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            ON CONFLICT(phone) DO UPDATE SET
                user_id = excluded.user_id,
                gender = excluded.gender,
                age = excluded.age,
                device_id = excluded.device_id,
                last_seen = excluded.last_seen,
                latest_record_id = excluded.latest_record_id,
                latest_summary_json = excluded.latest_summary_json
            """,
            (
                patient.get("phone", ""),
                patient.get("user_id", ""),
                patient.get("gender", ""),
                patient.get("age"),
                patient.get("device_id", ""),
                patient.get("first_seen", now_text()),
                patient.get("last_seen", now_text()),
                patient.get("latest_record_id", ""),
                json.dumps(patient.get("latest_summary", {}), ensure_ascii=False),
            ),
        )


def save_record_db(report: dict[str, Any], qrcode_token: str) -> None:
    init_database()
    with db_connect() as conn:
        conn.execute(
            """
            INSERT OR REPLACE INTO records (
                record_id, phone, user_id, device_id, gender, age, payload_json,
                report_json, last_summary_json, compare_summary_json,
                qrcode_token, qrcode_url, upload_remote, created_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                report.get("record_id", ""),
                report.get("phone", ""),
                report.get("user_id", ""),
                report.get("device_id", ""),
                report.get("gender", ""),
                report.get("age"),
                json.dumps(report.get("request_payload", {}), ensure_ascii=False),
                json.dumps(report, ensure_ascii=False),
                json.dumps(report.get("last_summary", {}), ensure_ascii=False),
                json.dumps(report.get("compare_summary", {}), ensure_ascii=False),
                qrcode_token,
                report.get("qrcode_url", ""),
                report.get("upload_remote", ""),
                int(report.get("created_at", int(time.time()))),
            ),
        )
        conn.execute(
            """
            INSERT OR REPLACE INTO qr_tokens (token, record_id, phone, expires_at, created_at)
            VALUES (?, ?, ?, ?, ?)
            """,
            (
                qrcode_token,
                report.get("record_id", ""),
                report.get("phone", ""),
                None,
                int(report.get("created_at", int(time.time()))),
            ),
        )


def row_to_record(row: sqlite3.Row) -> dict[str, Any]:
    report = json.loads(row["report_json"])
    report["qrcode_token"] = row["qrcode_token"]
    return report


def get_patient_records_db(phone: str) -> list[dict[str, Any]]:
    init_database()
    with db_connect() as conn:
        rows = conn.execute(
            "SELECT * FROM records WHERE phone = ? ORDER BY created_at DESC",
            (phone,),
        ).fetchall()
    return [row_to_record(row) for row in rows]


def get_record_by_token_db(token: str) -> dict[str, Any] | None:
    init_database()
    with db_connect() as conn:
        row = conn.execute(
            """
            SELECT records.* FROM records
            JOIN qr_tokens ON qr_tokens.record_id = records.record_id
            WHERE qr_tokens.token = ?
            """,
            (token,),
        ).fetchone()
    return row_to_record(row) if row else None


def issue_verification_code(phone: str) -> str:
    init_database()
    code = f"{secrets.randbelow(1000000):06d}"
    now = int(time.time())
    with db_connect() as conn:
        conn.execute(
            """
            INSERT INTO verification_codes (phone, code, expires_at, used, created_at)
            VALUES (?, ?, ?, 0, ?)
            """,
            (phone, code, now + 300, now),
        )
    return code


def verify_code(phone: str, code: str) -> bool:
    init_database()
    now = int(time.time())
    with db_connect() as conn:
        row = conn.execute(
            """
            SELECT id FROM verification_codes
            WHERE phone = ? AND code = ? AND used = 0 AND expires_at >= ?
            ORDER BY created_at DESC LIMIT 1
            """,
            (phone, code, now),
        ).fetchone()
        if not row:
            return False
        conn.execute("UPDATE verification_codes SET used = 1 WHERE id = ?", (row["id"],))
    return True


def create_mobile_session(phone: str) -> str:
    init_database()
    token = build_mobile_session_token()
    now = int(time.time())
    with db_connect() as conn:
        conn.execute(
            "INSERT INTO sessions (token, phone, expires_at, created_at) VALUES (?, ?, ?, ?)",
            (token, phone, now + 86400 * 30, now),
        )
    return token


def phone_by_session_token(token: str) -> str | None:
    if not token:
        return None
    init_database()
    with db_connect() as conn:
        row = conn.execute(
            "SELECT phone FROM sessions WHERE token = ? AND expires_at >= ?",
            (token, int(time.time())),
        ).fetchone()
    return str(row["phone"]) if row else None


def phone_from_session() -> str:
    token = request.headers.get("X-Session-Token", "").strip()
    if not token:
        abort(401, description="X-Session-Token required")
    phone = phone_by_session_token(token)
    if not phone:
        abort(401, description="invalid session")
    return phone


def phone_from_mobile_cookie() -> str | None:
    token = request.cookies.get("mobile_session", "").strip()
    return phone_by_session_token(token)


def default_runtime_state() -> dict[str, Any]:
    return {
        "fetch_count": 0,
        "last_fetch_time": "",
        "last_fetch_remote": "",
    }


def load_runtime_state() -> dict[str, Any]:
    ensure_storage()
    try:
        data = json.loads(RUNTIME_STATE_FILE.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else default_runtime_state()
    except json.JSONDecodeError:
        return default_runtime_state()


def save_runtime_state(data: dict[str, Any]) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    RUNTIME_STATE_FILE.write_text(
        json.dumps(data, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def default_pending_record() -> dict[str, Any]:
    return {
        "cmd": "upload_record",
        "device_id": "RA8P1-001",
        "phone": "13800138000",
        "gender": "male",
        "age": 35,
        "record": {
            "wangzhen": {},
            "wenzhen_audio": {},
            "wenzhen_question": {
                "sleep": "poor",
                "fatigue": "often",
            },
            "qiezhen": {},
            "summary": {
                "main_tendency": "气虚倾向",
            },
        },
    }


def load_pending_record() -> dict[str, Any]:
    ensure_storage()
    try:
        data = json.loads(PENDING_RECORD_FILE.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else default_pending_record()
    except json.JSONDecodeError:
        return default_pending_record()


def save_pending_record(payload: dict[str, Any]) -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    PENDING_RECORD_FILE.write_text(
        json.dumps(payload, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )


def pending_form_data(payload: dict[str, Any]) -> dict[str, Any]:
    question = payload.get("record", {}).get("wenzhen_question", {})
    summary = payload.get("record", {}).get("summary", {})
    return {
        "device_id": payload.get("device_id", ""),
        "phone": payload.get("phone", ""),
        "gender": payload.get("gender", "male"),
        "age": payload.get("age", ""),
        "sleep": question.get("sleep", ""),
        "fatigue": question.get("fatigue", ""),
        "main_tendency": summary.get("main_tendency", ""),
    }


def build_pending_record_from_form(form: Any) -> dict[str, Any]:
    age_text = str(form.get("age", "")).strip()
    age = int(age_text) if age_text.isdigit() else 0
    return {
        "cmd": "upload_record",
        "device_id": str(form.get("device_id", "")).strip(),
        "phone": str(form.get("phone", "")).strip(),
        "gender": str(form.get("gender", "male")).strip() or "male",
        "age": age,
        "record": {
            "wangzhen": {},
            "wenzhen_audio": {},
            "wenzhen_question": {
                "sleep": str(form.get("sleep", "")).strip(),
                "fatigue": str(form.get("fatigue", "")).strip(),
            },
            "qiezhen": {},
            "summary": {
                "main_tendency": str(form.get("main_tendency", "")).strip(),
            },
        },
    }


def compact_json(data: dict[str, Any]) -> str:
    return json.dumps(data, ensure_ascii=False, separators=(",", ":"))


def build_qrcode_url(record_id: str) -> str:
    return f"{BASE_URL}/ui/report/{record_id}"


def build_user_id(phone: str) -> str:
    digits = "".join(ch for ch in phone if ch.isdigit())
    suffix = digits[-4:] if digits else "0001"
    return f"u_{suffix}"


def build_record_id() -> str:
    return f"r_{int(time.time() * 1000)}"


def now_text() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())


def mask_phone(phone: str) -> str:
    digits = "".join(ch for ch in phone if ch.isdigit())
    if len(digits) >= 11:
        return f"{digits[:3]}****{digits[-4:]}"
    if len(digits) >= 7:
        return f"{digits[:3]}****{digits[-2:]}"
    return phone


def compare_summary_brief(report: dict[str, Any]) -> str:
    items = report.get("compare_summary", {}).get("items", [])
    if isinstance(items, list) and items:
        return str(items[-1])
    return str(report.get("last_summary", {}).get("main_tendency", ""))


def api_record_summary(report: dict[str, Any]) -> dict[str, Any]:
    record_id = str(report.get("record_id", ""))
    return {
        "record_id": record_id,
        "created_at": report.get("last_summary", {}).get("time", ""),
        "main_tendency": report.get("last_summary", {}).get("main_tendency", ""),
        "summary_brief": compare_summary_brief(report),
        "report_url": f"/ui/report/{record_id}",
        "qrcode_url": report.get("qrcode_url", ""),
    }


def api_report_payload(report: dict[str, Any]) -> dict[str, Any]:
    payload = report.get("request_payload", {})
    return {
        "ok": True,
        "record_id": report.get("record_id", ""),
        "user_id": report.get("user_id", ""),
        "device_id": report.get("device_id", ""),
        "created_at": report.get("last_summary", {}).get("time", ""),
        "phone_masked": mask_phone(str(report.get("phone", ""))),
        "gender": report.get("gender", ""),
        "age": report.get("age"),
        "record": payload.get("record", {}) if isinstance(payload, dict) else {},
        "last_summary": report.get("last_summary", {}),
        "compare_summary": report.get("compare_summary", {}),
        "qrcode_url": report.get("qrcode_url", ""),
    }


def build_patient_archive(
    phone: str,
    payload: dict[str, Any],
    reports: dict[str, Any],
) -> dict[str, Any]:
    old_record_ids = [
        record_id
        for record_id, report in sorted(
            reports.items(),
            key=lambda item: item[1].get("created_at", 0),
        )
        if report.get("phone") == phone
    ]
    return {
        "user_id": build_user_id(phone),
        "phone": phone,
        "gender": str(payload.get("gender", "")).strip(),
        "age": payload.get("age"),
        "device_id": str(payload.get("device_id", "")).strip(),
        "first_seen": now_text(),
        "last_seen": now_text(),
        "record_ids": old_record_ids,
        "latest_summary": {},
    }


def get_previous_patient_record(
    patient: dict[str, Any],
    reports: dict[str, Any],
) -> dict[str, Any] | None:
    for record_id in reversed(patient.get("record_ids", [])):
        report = reports.get(record_id)
        if report:
            return report
    return None


def analyze_main_tendency(payload: dict[str, Any]) -> str:
    record = payload.get("record", {})
    if not isinstance(record, dict):
        return "待进一步分析"

    summary = record.get("summary", {})
    if isinstance(summary, dict):
        tendency = summary.get("main_tendency")
        if isinstance(tendency, str) and tendency.strip():
            return tendency.strip()

    question = record.get("wenzhen_question", {})
    if isinstance(question, dict):
        fatigue = str(question.get("fatigue", "")).strip().lower()
        sleep = str(question.get("sleep", "")).strip().lower()
        if fatigue in {"high", "often", "yes"} or sleep in {"poor", "bad"}:
            return "气虚倾向"

    return "基本平稳"


def build_compare_summary(previous: dict[str, Any] | None, current_tendency: str) -> dict[str, Any]:
    if not previous:
        return {
            "title": "与上次相比",
            "items": [
                "这是该手机号的首条记录",
                "已建立本地随访基线",
                f"当前综合倾向为{current_tendency}",
            ],
        }

    previous_tendency = previous.get("last_summary", {}).get("main_tendency", "未知")
    if previous_tendency == current_tendency:
        tendency_text = "综合倾向较上次变化不大"
    else:
        tendency_text = f"综合倾向由{previous_tendency}调整为{current_tendency}"

    return {
        "title": "与上次相比",
        "items": [
            "舌象红润度略有变化",
            "问诊疲劳相关回答减少",
            tendency_text,
        ],
    }


def validate_upload_payload(data: Any) -> dict[str, Any]:
    if not isinstance(data, dict):
        abort(400, description="JSON body required")

    required_fields = ["cmd", "device_id", "phone", "gender", "age", "record"]
    for field in required_fields:
        if field not in data:
            abort(400, description=f"{field} is required")

    if data.get("cmd") != "upload_record":
        abort(400, description="cmd must be upload_record")

    if not isinstance(data.get("record"), dict):
        abort(400, description="record must be an object")

    return data


def create_upload_result(payload: dict[str, Any]) -> dict[str, Any]:
    reports = load_reports()
    patients = load_patients()
    phone = str(payload.get("phone", "")).strip()

    patient = patients.get(phone)
    if not isinstance(patient, dict):
        patient = build_patient_archive(phone, payload, reports)
    previous = get_previous_patient_record(patient, reports)

    current_tendency = analyze_main_tendency(payload)
    record_id = build_record_id()
    user_id = str(patient.get("user_id") or build_user_id(phone))
    last_summary = {
        "time": now_text(),
        "main_tendency": current_tendency,
    }
    compare_summary = build_compare_summary(previous, current_tendency)
    qrcode_token = build_qrcode_token()
    qrcode_url = build_qrcode_url(record_id)

    result = {
        "cmd": "upload_record_result",
        "ok": True,
        "user_id": user_id,
        "record_id": record_id,
        "last_summary": last_summary,
        "compare_summary": compare_summary,
        "qrcode_url": qrcode_url,
        "qrcode_token": qrcode_token,
    }

    report = {
        "record_id": record_id,
        "user_id": user_id,
        "device_id": str(payload.get("device_id", "")).strip(),
        "phone": phone,
        "gender": str(payload.get("gender", "")).strip(),
        "age": payload.get("age"),
        "request_payload": payload,
        "request_json": json.dumps(payload, ensure_ascii=False, indent=2),
        "last_summary": last_summary,
        "compare_summary": compare_summary,
        "qrcode_url": qrcode_url,
        "qrcode_token": qrcode_token,
        "upload_remote": request.remote_addr or "",
        "created_at": int(time.time()),
    }
    reports[record_id] = report

    record_ids = patient.get("record_ids", [])
    if not isinstance(record_ids, list):
        record_ids = []
    record_ids.append(record_id)
    patient.update(
        {
            "user_id": user_id,
            "phone": phone,
            "gender": report["gender"],
            "age": report["age"],
            "device_id": report["device_id"],
            "last_seen": last_summary["time"],
            "record_ids": record_ids,
            "latest_record_id": record_id,
            "latest_summary": last_summary,
        }
    )
    patient.setdefault("first_seen", last_summary["time"])
    patients[phone] = patient

    save_reports(reports)
    save_patients(patients)
    upsert_patient_db(patient)
    save_record_db(report, qrcode_token)
    return result


def recent_records() -> list[dict[str, Any]]:
    reports = list(load_reports().values())
    reports.sort(key=lambda item: item.get("created_at", 0), reverse=True)
    return reports


def patient_detail(phone: str) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    patients = load_patients()
    patient = patients.get(phone)
    if not isinstance(patient, dict):
        abort(404, description="patient not found")

    reports = load_reports()
    records = [
        reports[record_id]
        for record_id in patient.get("record_ids", [])
        if record_id in reports
    ]
    records.sort(key=lambda item: item.get("created_at", 0), reverse=True)
    return patient, records


@app.get("/")
def portal_home() -> Any:
    return render_template_string(PORTAL_TEMPLATE)


@app.get("/terminal")
def ui_home() -> Any:
    pending = load_pending_record()
    return render_template_string(
        HOME_TEMPLATE,
        base_url=BASE_URL,
        pending=pending_form_data(pending),
        pending_json=json.dumps(pending, ensure_ascii=False, indent=2),
        runtime=load_runtime_state(),
        records=recent_records()[:10],
    )


@app.post("/ui/pending-record")
def ui_pending_record() -> Any:
    payload = build_pending_record_from_form(request.form)
    save_pending_record(payload)
    create_upload_result(payload)
    return redirect(url_for("ui_home"))


@app.get("/ui/report/<record_id>")
def ui_report(record_id: str) -> Any:
    report = load_reports().get(record_id)
    if not report:
        abort(404, description="record not found")
    return render_template_string(REPORT_TEMPLATE, report=report)


@app.get("/r/<qrcode_token>")
def ui_qrcode_report(qrcode_token: str) -> Any:
    report = get_record_by_token_db(qrcode_token)
    if not report:
        abort(404, description="token not found")
    return render_template_string(REPORT_TEMPLATE, report=report)


@app.get("/ui/patient/<phone>")
def ui_patient(phone: str) -> Any:
    patient, records = patient_detail(phone)
    return render_template_string(PATIENT_TEMPLATE, patient=patient, records=records)


@app.get("/ui/patient/")
def ui_patient_missing_phone() -> Any:
    return redirect(url_for("mobile_home"))


@app.get("/mobile")
def mobile_home() -> Any:
    phone = phone_from_mobile_cookie()
    if phone:
        return redirect(url_for("mobile_records"))
    return render_template_string(MOBILE_LOGIN_TEMPLATE, phone="", dev_code="", error="")


@app.get("/mobile/manifest.json")
def mobile_manifest() -> Any:
    return jsonify(
        {
            "name": "四诊健康报告",
            "short_name": "四诊报告",
            "start_url": "/mobile",
            "scope": "/mobile",
            "display": "standalone",
            "background_color": "#f6f0e7",
            "theme_color": "#8b4a27",
            "description": "患者查看四诊检测结果和历史记录的手机端入口",
            "icons": [
                {
                    "src": "/mobile/icon.svg",
                    "sizes": "any",
                    "type": "image/svg+xml",
                    "purpose": "any maskable",
                }
            ],
        }
    )


@app.get("/mobile/icon.svg")
def mobile_icon() -> Any:
    svg = """<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128">
<rect width="128" height="128" rx="28" fill="#8b4a27"/>
<circle cx="64" cy="64" r="43" fill="#fffaf3"/>
<path d="M64 31c16 14 24 27 24 39 0 15-11 27-24 27S40 85 40 70c0-12 8-25 24-39Z" fill="#d8a15d"/>
<path d="M50 70h28M64 54v32" stroke="#6f3218" stroke-width="8" stroke-linecap="round"/>
</svg>"""
    return app.response_class(svg, mimetype="image/svg+xml")


@app.get("/mobile/sw.js")
def mobile_service_worker() -> Any:
    js = """
self.addEventListener("install", function (event) {
  self.skipWaiting();
});
self.addEventListener("activate", function (event) {
  event.waitUntil(self.clients.claim());
});
"""
    return app.response_class(js, mimetype="application/javascript")


@app.post("/mobile/send-code")
def mobile_send_code() -> Any:
    phone = str(request.form.get("phone", "")).strip()
    if not phone:
        return render_template_string(
            MOBILE_LOGIN_TEMPLATE,
            phone="",
            dev_code="",
            error="请输入手机号。",
        )
    code = issue_verification_code(phone)
    return render_template_string(
        MOBILE_LOGIN_TEMPLATE,
        phone=phone,
        dev_code=code,
        error="",
    )


@app.post("/mobile/login")
def mobile_login() -> Any:
    phone = str(request.form.get("phone", "")).strip()
    code = str(request.form.get("code", "")).strip()
    if not phone or not code:
        return render_template_string(
            MOBILE_LOGIN_TEMPLATE,
            phone=phone,
            dev_code="",
            error="手机号和验证码都需要填写。",
        )
    if not verify_code(phone, code):
        return render_template_string(
            MOBILE_LOGIN_TEMPLATE,
            phone=phone,
            dev_code="",
            error="验证码不正确或已过期。",
        )

    response = make_response(redirect(url_for("mobile_records")))
    response.set_cookie(
        "mobile_session",
        create_mobile_session(phone),
        max_age=86400 * 30,
        httponly=True,
        samesite="Lax",
    )
    return response


@app.get("/mobile/records")
def mobile_records() -> Any:
    phone = phone_from_mobile_cookie()
    if not phone:
        return redirect(url_for("mobile_home"))
    records = get_patient_records_db(phone)
    return render_template_string(MOBILE_RECORDS_TEMPLATE, phone=phone, records=records)


@app.get("/mobile/report/<record_id>")
def mobile_report(record_id: str) -> Any:
    phone = phone_from_mobile_cookie()
    if not phone:
        return redirect(url_for("mobile_home"))
    for report in get_patient_records_db(phone):
        if report.get("record_id") == record_id:
            return render_template_string(REPORT_TEMPLATE, report=report)
    abort(404, description="record not found")


@app.get("/health")
def health() -> Any:
    return jsonify({"status": "ok", "service": "aliyun_cloud"})


@app.get("/api/test-record")
def api_test_record() -> Any:
    state = load_runtime_state()
    state["fetch_count"] = int(state.get("fetch_count", 0)) + 1
    state["last_fetch_time"] = now_text()
    state["last_fetch_remote"] = request.remote_addr or ""
    save_runtime_state(state)
    return app.response_class(
        compact_json(load_pending_record()),
        mimetype="application/json",
    )


@app.post("/api/test-record")
def api_set_test_record() -> Any:
    payload = validate_upload_payload(request.get_json(silent=True))
    save_pending_record(payload)
    return jsonify({"ok": True})


@app.post("/api/upload-record")
def api_upload_record() -> Any:
    payload = validate_upload_payload(request.get_json(silent=True))
    result = create_upload_result(payload)
    return jsonify(result)


@app.post("/api/diagnosis")
def api_diagnosis() -> Any:
    payload = validate_upload_payload(request.get_json(silent=True))
    result = create_upload_result(payload)
    return jsonify(result)


@app.get("/report/<record_id>")
def api_report(record_id: str) -> Any:
    report = load_reports().get(record_id)
    if not report:
        abort(404, description="record not found")
    response = api_report_payload(report)
    response["raw_report"] = report
    return jsonify(response)


@app.get("/api/patient/<phone>")
def api_patient(phone: str) -> Any:
    patient, records = patient_detail(phone)
    user = {
        "user_id": patient.get("user_id", ""),
        "phone_masked": mask_phone(phone),
        "gender": patient.get("gender", ""),
        "age": patient.get("age"),
        "created_at": patient.get("first_seen", ""),
        "record_count": len(records),
    }
    return jsonify(
        {
            "ok": True,
            "user": user,
            "records": [api_record_summary(record) for record in records],
            "patient": patient,
            "raw_records": records,
        }
    )


@app.post("/api/mobile/send-code")
def api_mobile_send_code() -> Any:
    data = request.get_json(silent=True) or {}
    phone = str(data.get("phone", "")).strip()
    if not phone:
        abort(400, description="phone is required")
    code = issue_verification_code(phone)
    return jsonify(
        {
            "ok": True,
            "phone": phone,
            "expires_in": 300,
            "dev_code": code,
        }
    )


@app.post("/api/mobile/login")
def api_mobile_login() -> Any:
    data = request.get_json(silent=True) or {}
    phone = str(data.get("phone", "")).strip()
    code = str(data.get("code", "")).strip()
    if not phone or not code:
        abort(400, description="phone and code are required")
    if not verify_code(phone, code):
        abort(401, description="invalid code")
    token = create_mobile_session(phone)
    records = get_patient_records_db(phone)
    return jsonify(
        {
            "ok": True,
            "session_token": token,
            "phone": phone,
            "record_count": len(records),
        }
    )


@app.get("/api/mobile/records")
def api_mobile_records() -> Any:
    phone = phone_from_session()
    records = get_patient_records_db(phone)
    items = [
        {
            "record_id": record.get("record_id"),
            "time": record.get("last_summary", {}).get("time"),
            "main_tendency": record.get("last_summary", {}).get("main_tendency"),
            "qrcode_url": record.get("qrcode_url"),
        }
        for record in records
    ]
    return jsonify({"ok": True, "phone": phone, "records": items})


@app.get("/api/mobile/report/<record_id>")
def api_mobile_report(record_id: str) -> Any:
    phone = phone_from_session()
    reports = get_patient_records_db(phone)
    for report in reports:
        if report.get("record_id") == record_id:
            return jsonify({"ok": True, "report": report})
    abort(404, description="record not found")


@app.get("/api/qrcode/<qrcode_token>")
def api_qrcode_report(qrcode_token: str) -> Any:
    report = get_record_by_token_db(qrcode_token)
    if not report:
        abort(404, description="token not found")
    return jsonify({"ok": True, "report": report})


if __name__ == "__main__":
    ensure_storage()
    app.run(host=HOST, port=PORT)
