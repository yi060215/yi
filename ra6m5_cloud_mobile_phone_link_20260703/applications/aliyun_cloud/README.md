# 中医四诊本地云端后端

这是当前 `RA6M5 + ESP32-C3` 联调用的 Flask 后端。它用于接收板端通过 WiFi 上传的四诊记录，并按手机号自动建档、保存历史记录、生成前后对比摘要，同时提供手机端登录、历史记录、报告查看和二维码 token 接口。

当前定位：本地演示云端。后续可以迁移到阿里云、腾讯云或其他公网服务器。

## 功能

- 首页填写“待上传记录”，板端可通过接口拉取。
- RA6M5/ESP32-C3 上传本次四诊记录。
- 后端按手机号自动创建或更新患者档案。
- 后端保存每次上传的历史记录。
- 同一手机号第二次及以后上传时，自动生成前后对比摘要。
- 提供报告页面和患者档案页面，方便演示和调试。

## 目录说明

```text
applications/aliyun_cloud/
├── app.py                 Flask 后端主程序
├── requirements.txt       Python 依赖
├── README.md              本说明文档
├── .env.example           环境变量示例
├── .gitignore             忽略本地运行数据、数据库和虚拟环境
├── run_server.sh          Linux 手动启动脚本
├── deploy_aliyun.sh       Linux 部署辅助脚本
├── tcm-cloud.service      systemd 服务示例
└── data/                  本地数据目录，包含 SQLite 和调试 JSON
```

## 本地运行

Windows PowerShell：

```powershell
cd D:\RA6M5\wifi_module_keil\applications\aliyun_cloud
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python app.py
```

默认启动地址：

```text
http://127.0.0.1:5000/
```

## 自检

不启动浏览器也可以先跑一次后端自检：

```powershell
cd D:\RA6M5\wifi_module_keil\applications\aliyun_cloud
.\.venv\Scripts\python.exe smoke_test.py
```

自检会连续上传同一个测试手机号的两条记录，并确认：

- `/api/upload-record` 可以写入报告。
- `/api/patient/<phone>` 可以查到患者档案。
- 同一手机号历史记录数为 2。
- 第二条记录会生成前后对比摘要。
- `/api/mobile/send-code` 可以生成登录验证码。
- `/api/mobile/login` 可以返回手机端会话 token。
- `/api/mobile/records` 可以查询手机端历史记录。
- `/api/qrcode/<token>` 可以通过二维码 token 查询报告。

如果板子和电脑在同一个 WiFi 下，板端不要使用 `127.0.0.1`，应使用电脑局域网 IP，例如：

```text
http://192.168.x.x:5000/
```

## 环境变量

可选环境变量：

```text
APP_HOST=0.0.0.0
APP_PORT=5000
BASE_URL=http://127.0.0.1:5000
```

`BASE_URL` 用于生成报告链接和二维码链接。部署到公网服务器后，应改为公网 IP 或域名。

## 主要页面

首页：

```text
GET /
```

单次报告页面：

```text
GET /ui/report/<record_id>
```

患者档案页面：

```text
GET /ui/patient/<phone>
```

注意：患者档案页面必须带手机号，`/ui/patient/` 不能直接打开具体档案。

## 主要接口

健康检查：

```http
GET /health
```

板端拉取待上传记录：

```http
GET /api/test-record
```

网页或调试工具设置待上传记录：

```http
POST /api/test-record
```

板端上传本次四诊记录：

```http
POST /api/upload-record
```

查询患者档案和历史记录：

```http
GET /api/patient/<phone>
```

查询单条报告：

```http
GET /report/<record_id>
```

手机端发送验证码：

```http
POST /api/mobile/send-code
```

请求示例：

```json
{
  "phone": "13800138000"
}
```

当前演示版会在返回里带 `dev_code`，正式接短信平台后应删除该字段。

手机端手机号登录：

```http
POST /api/mobile/login
```

请求示例：

```json
{
  "phone": "13800138000",
  "code": "123456"
}
```

返回示例：

```json
{
  "ok": true,
  "session_token": "SESSION_TOKEN",
  "phone": "13800138000",
  "record_count": 2
}
```

手机端查询历史记录：

```http
GET /api/mobile/records
X-Session-Token: SESSION_TOKEN
```

手机端查询某次报告：

```http
GET /api/mobile/report/<record_id>
X-Session-Token: SESSION_TOKEN
```

二维码 token 查询报告：

```http
GET /api/qrcode/<qrcode_token>
```

二维码网页入口：

```http
GET /r/<qrcode_token>
```

## 上传数据格式

```json
{
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
      "fatigue": "often"
    },
    "qiezhen": {},
    "summary": {
      "main_tendency": "气虚倾向"
    }
  }
}
```

## 返回数据格式

```json
{
  "cmd": "upload_record_result",
  "ok": true,
  "user_id": "u_8000",
  "record_id": "r_1783014602673",
  "last_summary": {
    "time": "2026-07-03 10:20:00",
    "main_tendency": "气虚倾向"
  },
  "compare_summary": {
    "title": "与上次相比",
    "items": [
      "舌象红润度略有变化",
      "问诊疲劳相关回答减少",
      "综合倾向较上次变化不大"
    ]
  },
  "qrcode_url": "http://127.0.0.1:5000/ui/report/r_1783014602673"
}
```

## 数据库和数据文件

当前已经新增 SQLite 数据库：

```text
data/tcm_cloud.db
```

SQLite 表结构包括：

```text
patients              用户档案
records               四诊记录
verification_codes    登录验证码
qr_tokens             二维码 token
sessions              手机端登录会话
```

为了兼容前期演示页面，目前仍保留 JSON 调试文件：

```text
data/reports.json          每次上传的完整报告记录
data/patients.json         按手机号建立的患者档案
data/pending_record.json   首页配置的待上传记录
data/runtime_state.json    板端拉取状态
```

正式部署时可以先继续使用 SQLite；如果后续多人并发或长期运行，再迁移到 MySQL 或云数据库。

## 板端配置

板端配置文件：

```text
D:\RA6M5\wifi_module_keil\include\config.h
```

本地联调时，服务器地址应填写电脑局域网 IP：

```c
#define CLOUD_SERVER_HOST   "192.168.x.x"
#define CLOUD_SERVER_PORT   5000
```

部署到公网服务器后，改成公网 IP 或域名。

## 部署到云服务器

推荐系统：Ubuntu 22.04。

上传本目录到服务器后：

```bash
chmod +x run_server.sh deploy_aliyun.sh
./deploy_aliyun.sh
```

部署前需要修改 `tcm-cloud.service` 里的路径和公网地址：

```ini
Environment=BASE_URL=http://YOUR_PUBLIC_IP:5000
```

然后重启服务：

```bash
sudo systemctl restart tcm-cloud
sudo systemctl status tcm-cloud --no-pager
```

## 当前注意事项

- 当前后端是本地演示服务，不是正式公网云端。
- GitHub 上传时不要提交 `.venv`、日志文件、本地真实数据、真实手机号和 `data/tcm_cloud.db`。
- 当前对比摘要是演示逻辑，主要比较 `summary.main_tendency`，后续可接入真实图像、音频、问诊和脉象分析结果。
- 当前验证码接口返回 `dev_code` 只是为了本地联调，正式上线应改成短信服务发送，不再返回验证码明文。

## 手机网页端入口

为了方便队长和手机端同学直接看效果，当前后端已经提供一个最小手机网页端：

```text
GET /mobile
```

它可以完成：

- 手机号验证码登录。
- 查看当前手机号下的历史四诊记录。
- 查看某一次四诊报告。
- 打开二维码 token 分享链接。

这是演示版手机端，不替代后续正式 App/小程序，但可以作为手机端接口对接前的可视化验证入口。
