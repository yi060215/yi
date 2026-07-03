@echo off
setlocal

rem Restore ESP32-C3 MINI-1 ESP-AT firmware with AT UART on GPIO0/GPIO1.
rem AT_TX = GPIO0, AT_RX = GPIO1, hardware flow control disabled.

set "ESP_PORT=COM14"
set "SCRIPT_DIR=%~dp0"
set "IMAGE=%SCRIPT_DIR%esp32c3_custom\factory_MINI-1_uart0_1_noflow.bin"

if not exist "%IMAGE%" (
    echo [ERROR] Image not found:
    echo %IMAGE%
    exit /b 1
)

echo [INFO] Port : %ESP_PORT%
echo [INFO] Image: %IMAGE%
echo [INFO] Flash address: 0x0

python -m esptool --chip esp32c3 --port %ESP_PORT% --baud 115200 ^
  --before default_reset --after hard_reset write_flash ^
  --flash_mode dio --flash_freq 40m --flash_size 4MB ^
  0x0 "%IMAGE%"

endlocal
