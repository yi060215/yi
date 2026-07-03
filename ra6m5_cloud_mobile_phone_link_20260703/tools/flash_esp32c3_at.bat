@echo off
chcp 65001 >nul
setlocal

rem Change this to the USB-UART COM port connected to ESP32-C3 UART0.
set ESP_PORT=COM3

set AT_DIR=D:\RA6M5\中医四诊\92120\资料\ESP32-C3资料\ESP32-C3-MINI-1-V2.4.2.0

pushd "%AT_DIR%" || exit /b 1

python -m esptool --chip esp32c3 --port %ESP_PORT% --baud 460800 ^
  --before default_reset --after hard_reset write_flash ^
  --flash_mode dio --flash_freq 40m --flash_size 4MB ^
  0x8000 partition_table/partition-table.bin ^
  0xd000 ota_data_initial.bin ^
  0xf000 phy_multiple_init_data.bin ^
  0x0 bootloader/bootloader.bin ^
  0x60000 esp-at.bin ^
  0x1e000 at_customize.bin ^
  0x25000 customized_partitions/server_cert.bin ^
  0x3a000 customized_partitions/mqtt_key.bin ^
  0x27000 customized_partitions/server_key.bin ^
  0x29000 customized_partitions/server_ca.bin ^
  0x2f000 customized_partitions/client_ca.bin ^
  0x31000 customized_partitions/factory_param.bin ^
  0x1F000 customized_partitions/ble_data.bin ^
  0x3c000 customized_partitions/mqtt_ca.bin ^
  0x38000 customized_partitions/mqtt_cert.bin ^
  0x2b000 customized_partitions/client_cert.bin ^
  0x2d000 customized_partitions/client_key.bin

popd
endlocal
