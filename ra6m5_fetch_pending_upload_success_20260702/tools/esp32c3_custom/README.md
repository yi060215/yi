# ESP32-C3 AT UART20/21 Test Firmware

Purpose: verify AT command communication on the ESP32-C3 classic board USB/U0 pins.

Generated from the original file:

`92120\资料\ESP32-C3资料\ESP32-C3-MINI-1-V2.4.2.0\factory\factory_MINI-1.bin`

Only the `factory_param` area at flash offset `0x31000` was changed.

## Files

- `factory_MINI-1_uart20_21_noflow.bin`
  - Full 4 MB factory image.
  - Flash address: `0x0`.
- `factory_param_uart20_21_noflow.bin`
  - Only the 4 KB factory parameter partition.
  - Flash address: `0x31000`.

## Patched Parameters

- UART port: `1`
- Baudrate: `115200`
- AT TX pin: `GPIO21`
- AT RX pin: `GPIO20`
- CTS pin: disabled (`255`)
- RTS pin: disabled (`255`)

## Recommended Test

Flash the full image first:

```text
0x0  factory_MINI-1_uart20_21_noflow.bin
```

After flashing, reset the ESP32-C3 and test on the same COM port:

```powershell
cd D:\RA6M5\中医四诊\firmware\wifi_module
powershell -ExecutionPolicy Bypass -File .\tools\send_esp32_at.ps1 -Port COM14 -Command AT
```

Expected response:

```text
OK
```
