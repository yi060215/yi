# WiFi Module

This folder isolates the ESP32-C3 AT-based WiFi stack from the rest of the project.

Use these files in a standalone Keil group:
- `drivers/drv_esp32.c`
- `drivers/drv_esp32.h`
- `drivers/drv_wifi.c`
- `drivers/drv_wifi.h`
- `devices/wifi_bluetooth/dev_wifi_bt.c`
- `devices/wifi_bluetooth/dev_wifi_bt.h`
- `src/wifi_bringup_test.c`

Keep the main application and unrelated modules out of this group.

Bring-up order:
1. `esp32_init()`
2. `wifi_connect()`
3. `wifi_get_ip()`
4. `cloud_fetch_questions()`
