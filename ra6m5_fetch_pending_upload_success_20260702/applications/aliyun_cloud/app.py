from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any

from flask import Flask, abort, jsonify, redirect, render_template_string, request, url_for


APP_ROOT = Path(__file__).resolve().parent
DATA_DIR = APP_ROOT / "data"
REPORTS_FILE = DATA_DIR / "reports.json"
PENDING_RECORD_FILE = DATA_DIR / "pending_record.json"
RUNTIME_STATE_FILE = DATA_DIR / "runtime_state.json"

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
      background:
        radial-gradient(circle at top right, rgba(139, 74, 39, 0.12), transparent 24%),
        linear-gradient(180deg, #f8f2ea, var(--bg));
    }
    .wrap {
      width: min(1180px, calc(100% - 28px));
      margin: 24px auto 40px;
    }
    .hero, .panel {
      background: var(--panel);
      border: 1px solid var(--line);
      border-radius: 18px;
      box-shadow: 0 18px 40px rgba(67, 46, 28, 0.08);
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
      background: linear-gradient(135deg, var(--accent), #7d3414);
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
          <button type="submit">保存待上传记录</button>
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


def ensure_storage() -> None:
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    if not REPORTS_FILE.exists():
        REPORTS_FILE.write_text("{}", encoding="utf-8")
    if not PENDING_RECORD_FILE.exists():
        save_pending_record(default_pending_record())
    if not RUNTIME_STATE_FILE.exists():
        save_runtime_state(default_runtime_state())


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
        "phone": "18800000000",
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
    return f"r_{int(time.time())}"


def now_text() -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime())


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
    phone = str(payload.get("phone", "")).strip()

    previous = None
    for item in sorted(reports.values(), key=lambda x: x.get("created_at", 0), reverse=True):
        if item.get("phone") == phone:
            previous = item
            break

    current_tendency = analyze_main_tendency(payload)
    record_id = build_record_id()
    user_id = build_user_id(phone)
    last_summary = {
        "time": now_text(),
        "main_tendency": current_tendency,
    }
    compare_summary = build_compare_summary(previous, current_tendency)
    qrcode_url = build_qrcode_url(record_id)

    result = {
        "cmd": "upload_record_result",
        "ok": True,
        "user_id": user_id,
        "record_id": record_id,
        "last_summary": last_summary,
        "compare_summary": compare_summary,
        "qrcode_url": qrcode_url,
    }

    reports[record_id] = {
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
        "upload_remote": request.remote_addr or "",
        "created_at": int(time.time()),
    }
    save_reports(reports)
    return result


def recent_records() -> list[dict[str, Any]]:
    reports = list(load_reports().values())
    reports.sort(key=lambda item: item.get("created_at", 0), reverse=True)
    return reports


@app.get("/")
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
    return redirect(url_for("ui_home"))


@app.get("/ui/report/<record_id>")
def ui_report(record_id: str) -> Any:
    report = load_reports().get(record_id)
    if not report:
        abort(404, description="record not found")
    return render_template_string(REPORT_TEMPLATE, report=report)


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
    return jsonify(report)


if __name__ == "__main__":
    ensure_storage()
    app.run(host=HOST, port=PORT)
