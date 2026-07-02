# Aliyun Cloud Service

用于当前 `RA6M5 + ESP32-C3` 联调的最小云端服务。

接口：
- `GET /health`
- `GET /api/questions`
- `POST /api/diagnosis`
- `POST /api/send-sms`
- `GET /report/<report_id>`

## Local Run

```powershell
cd D:\RA6M5\wifi_module_keil\applications\aliyun_cloud
python -m venv .venv
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
python app.py
```

默认端口：
- `5000`

## Aliyun Deploy

建议系统：
- `Ubuntu 22.04`

上传整个目录到服务器后执行：

```bash
chmod +x run_server.sh deploy_aliyun.sh
./deploy_aliyun.sh
```

部署后记得修改：
- `tcm-cloud.service`

把这行里的公网 IP 改掉：

```ini
Environment=BASE_URL=http://YOUR_PUBLIC_IP:5000
```

然后重启服务：

```bash
sudo systemctl restart tcm-cloud
sudo systemctl status tcm-cloud --no-pager
```

## Board Config

部署完成后，修改板端：
- `D:\RA6M5\wifi_module_keil\include\config.h`

改成：

```c
#define CLOUD_SERVER_HOST   "YOUR_PUBLIC_IP"
#define CLOUD_SERVER_PORT   5000
```

然后重新编译烧录。

## SMS

当前 `POST /api/send-sms` 还是占位实现。

这已经足够先跑通：
- 拉题
- 提交诊断
- 生成报告链接

下一步再替换成阿里云短信 SDK。
