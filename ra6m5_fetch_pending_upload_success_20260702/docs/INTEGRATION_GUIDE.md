# RA6M5 + ESP32-C3 WiFi 驱动 — 集成指南

## 文件说明

新创建的文件 (直接复制到你的 FSP 工程对应目录):
```
firmware/drivers/drv_esp32.h      → 工程的 drivers/
firmware/drivers/drv_esp32.c      → 工程的 drivers/
firmware/drivers/drv_wifi.h       → 工程的 drivers/
firmware/drivers/drv_wifi.c       → 工程的 drivers/
firmware/devices/wifi_bluetooth/  → 工程的 devices/wifi_bluetooth/ (新建文件夹)
firmware/app_wifi_test.c          → 工程的 src/ (测试代码, 可替换 app.c)
```

---

## 一、硬件接线

```
RA6M5                          ESP32-C3-MINI-1
─────────────────────────────────────────────────
P101 (TXD0)       ──────────►   RXD
P100 (RXD0)       ◄──────────   TXD
GND               ───────────   GND
3.3V              ───────────   VCC, EN (使能)
                                  IO0 上拉 (如果需进入 AT 模式)
```

> 注意: ESP32-C3 是 3.3V 器件, 直接用 RA6M5 的 3.3V 供电。不要接 5V!
> ESP32-C3 的 EN 引脚需要通过 10kΩ 上拉到 3.3V。

---

## 二、FSP 配置修改 (在 e² studio 或 RA Configurator 中操作)

### 2.1 添加 SCI0 UART (g_uart0)

打开 FSP Configuration, 在 **Stacks** 标签页中:

1. 点击 **New Stack** → **Connectivity** → **UART (r_sci_uart)**
2. 配置参数:

| 参数 | 值 |
|------|-----|
| Name | g_uart0 |
| Channel | 0 |
| Baud Rate | 115200 |
| Data Bits | 8 bits |
| Parity | None |
| Stop Bits | 1 bit |
| Clock Source | Internal Clock |
| Flow Control | Disable (全部关掉) |
| Callback | esp32_uart_callback |
| RX Interrupt Priority | Priority 12 |
| TX Interrupt Priority | Priority 12 |

### 2.2 配置引脚 (Pins 标签页)

找到 **SCI0** 相关引脚, 配置为 UART 功能:

| 引脚 | 信号 | 功能 |
|------|------|------|
| P101 | TXD0 (SDA0) | SCI0 TX |
| P100 | RXD0 (SCL0) | SCI0 RX |

### 2.3 点击 Generate Project Content

这会重新生成 `ra_gen/hal_data.c` 和 `ra_gen/hal_data.h`, 包含 g_uart0 的完整配置。

---

## 三、现有文件修改

### 3.1 `include/config.h`

```diff
- #define DEV_USE_WiFiBt      0
+ #define DEV_USE_WiFiBt      1
```

### 3.2 `drivers/drv_uart.c`

在 `UARTDrvInit`, `UARTDrvWrite`, `UARTDrvRead` 三个函数的 switch 中,
将 `case 0:` (原来是空的 break) 改为实际代码:

```diff
  static int UARTDrvInit(struct UartDev *ptdev)
  {
      switch(ptdev->channel)
      {
          case 0:
          {
+             fsp_err_t err = g_uart0.p_api->open(g_uart0.p_ctrl, g_uart0.p_cfg);
+             assert(FSP_SUCCESS == err);
              break;
          }
          case 7:
          {
              fsp_err_t err = g_uart7.p_api->open(g_uart7.p_ctrl, g_uart7.p_cfg);
              assert(FSP_SUCCESS == err);
              break;
          }
          ...
      }
  }
```

同样在 Write 和 Read 中添加 case 0 (参考 case 7 的实现)。

同时在 drv_uart.c 中添加 ESP32 设备的注册 (在 UartDevicesCreate 函数中):

```diff
+ /* ESP32 UART 设备 */
+ static volatile bool gEsp32UartTxCplt = false;
+ static volatile bool gEsp32UartRxCplt = false;
+
+ static struct UartDev gEsp32Device = {
+     .name    = "ESP32",
+     .channel = 0,
+     .Init    = UARTDrvInit,
+     .Read    = UARTDrvRead,
+     .Write   = UARTDrvWrite,
+     .next    = NULL
+ };

  void UartDevicesCreate(void)
  {
      UartDeviceInsert(&gLogDevice);
      gLogDevice.Init(&gLogDevice);
+     UartDeviceInsert(&gEsp32Device);
+     gEsp32Device.Init(&gEsp32Device);
  }
```

在 drv_uart.c 中添加 ESP32 的回调函数:

```c
void esp32_uart_callback(uart_callback_args_t *p_args)
{
    switch(p_args->event)
    {
        case UART_EVENT_RX_COMPLETE:
            gEsp32UartRxCplt = true;  // 注意: 如果 gEsp32UartRxCplt 是 static
            break;                    // 需要在 drv_esp32.c 中通过 extern 引用
        case UART_EVENT_TX_COMPLETE:
            gEsp32UartTxCplt = true;
            break;
        default: break;
    }
}
```

> 更简洁的做法: drv_esp32.c 中的回调 `esp32_uart_callback` 会通过 FSP 自动注册,
> 不需要在 drv_uart.c 重复。只需要确保 drv_uart.c 的 switch-case 中 channel 0 能正确调用 g_uart0。

### 3.3 keil 工程中添加源文件

在 Keil MDK 中, 将以下文件添加到工程:
- `drivers/drv_esp32.c`
- `drivers/drv_wifi.c`
- `devices/wifi_bluetooth/dev_wifi_bt.c`

右键 Target → Add Existing Files → 选择上述文件。

添加 include 路径 (Project → Options for Target → C/C++ → Include Paths):
- `..\drivers`
- `..\devices\wifi_bluetooth`

---

## 四、验证步骤

1. **上电检查**: 上电后 ESP32 模块指示灯应该亮起
2. **编译下载**: 编译工程, 下载到 RA6M5
3. **串口监视**: 用串口工具连接 SCI7 (115200), 观察调试输出
4. **预期输出**:
```
[ESP32] UART initialized (SCI0)
[ESP32] AT test OK, module online.
[WiFi] ESP32 initialized.
```

如果看到 `AT test FAILED`:
- 检查 ESP32-C3 的 EN 引脚是否上拉到 3.3V
- 检查 P100/P101 接线是否正确
- 用 USB-TTL 先单独测试 ESP32 模块是否能响应 AT 指令

5. **WiFi 连接测试**: 调用 `wifi_connect("你的WiFi名", "密码")`
6. **HTTP 测试**: 调用 `cloud_fetch_questions(resp, sizeof(resp))`

---

## 五、云端 API 格式参考

### GET /api/questions
Response:
```json
[
  {"id":1, "category":"望诊", "question":"请观察舌苔颜色", "options":["淡白","红","紫暗","黄"]},
  {"id":2, "category":"问诊", "question":"是否有头痛症状？", "options":["无","偶尔","经常","持续"]}
]
```

### POST /api/diagnosis
Request:
```json
{
  "patient_name": "张三",
  "patient_phone": "18800000000",
  "answers": [{"qid":1, "answer":"淡白"}, {"qid":2, "answer":"经常"}]
}
```
Response:
```json
{
  "report_id": "R20260629001",
  "diagnosis": "气血两虚，建议...",
  "recommendations": ["...", "..."]
}
```

### POST /api/send-sms
Request:
```json
{"phone": "18800000000", "report_id": "R20260629001"}
```

---

## 六、云端参考代码 (Flask)

```python
# cloud_server.py — 最简云端 API
from flask import Flask, request, jsonify
import requests  # 用于调用阿里云短信 API

app = Flask(__name__)

QUESTIONS = [
  {"id":1, "category":"望诊", "question":"舌苔颜色？", "options":["淡白","红","紫暗","黄"]},
  {"id":2, "category":"问诊", "question":"睡眠质量？", "options":["好","一般","差"]},
]

@app.route("/api/questions")
def api_questions():
    return jsonify(QUESTIONS)

@app.route("/api/diagnosis", methods=["POST"])
def api_diagnosis():
    data = request.get_json()
    # TODO: 接入你的诊断 AI/规则引擎
    report_id = f"R{int(time.time())}"
    return jsonify({
        "report_id": report_id,
        "diagnosis": "综合诊断结果...",
        "recommendations": ["建议1", "建议2"]
    })

@app.route("/api/send-sms", methods=["POST"])
def api_send_sms():
    data = request.get_json()
    phone = data["phone"]
    report_url = f"http://YOUR_SERVER_IP/report/{data['report_id']}"
    # TODO: 调用阿里云 SendSms API
    # send_aliyun_sms(phone, report_url)
    return jsonify({"status": "ok", "message": f"SMS sent to {phone}"})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
```
