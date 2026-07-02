# RA6M5 ESP32-C3 WiFi Upload Success Version

Date: 2026-07-02

This version verifies the complete path:

RA6M5 -> UART -> ESP32-C3 AT firmware -> WiFi -> local Flask backend

## Verified Result

- Backend URL used in local verification: `http://<backend_lan_ip>:5000`
- RA6M5/ESP32-C3 IP seen by backend: `<board_lan_ip>`
- Successful record id: `r_1782993604`
- Backend field: `upload_remote = <board_lan_ip>`
- Flow verified:
  - `GET /api/test-record`
  - Parse JSON body
  - `POST /api/upload-record`
  - Backend creates report record

## Hardware Wiring

- RA6M5 `P112 / TXD2` -> ESP32-C3 `GPIO1 / RX`
- RA6M5 `P113 / RXD2` -> ESP32-C3 `GPIO0 / TX`
- RA6M5 `GND` -> ESP32-C3 `GND`
- RA6M5 `3V3` -> ESP32-C3 `3V3`

PC debug UART:

- RA6M5 `SCI7`, pins `P613/P614`
- COM port used during verification: `COM6`
- Baud rate: `115200`

## Important Files

- Keil project: `WiFiModule.uvprojx`
- Board entry code: `src/hal_entry.c`
- WiFi/backend config: `include/config.h`
- Local backend: `applications/aliyun_cloud/app.py`
- Final build log: `build_fetch_pending_upload_jsonfix.log`
- Final flash log: `flash_fetch_pending_upload_jsonfix.log`
- Final AXF: `WiFiModule.axf`

## Before Running

Edit `include/config.h` locally:

```c
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"
#define CLOUD_SERVER_HOST   "YOUR_BACKEND_LAN_IP"
#define CLOUD_SERVER_PORT   5000
```

The GitHub version intentionally uses placeholders and does not include the real WiFi password.

## Current Board Behavior

After reset, RA6M5:

1. Initializes UART to ESP32-C3.
2. Sends AT commands to enter STA mode.
3. Connects WiFi using `WIFI_SSID` and `WIFI_PASSWORD` in `include/config.h`.
4. Connects to backend at `CLOUD_SERVER_HOST:CLOUD_SERVER_PORT`.
5. Fetches pending JSON from `/api/test-record`.
6. Uploads the fetched JSON to `/api/upload-record`.
7. Holds success state after upload succeeds.

## Notes For RA8P1 Migration

Porting mainly needs:

- Replace RA6M5 UART initialization/pins with RA8P1 UART used for ESP32-C3.
- Keep AT command sequence and HTTP request logic from `src/hal_entry.c`.
- Update WiFi/backend config in `include/config.h`.
- Keep ESP32-C3 AT firmware UART pair as verified: GPIO0/GPIO1.
