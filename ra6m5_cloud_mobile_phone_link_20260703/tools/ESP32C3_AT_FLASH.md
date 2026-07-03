# ESP32-C3 Classic AT Firmware Notes

This note is for the blue "ESP32C3 classic" board in the module manual.

Important facts from the manual:

- The classic board has an onboard USB-to-TTL chip.
- The onboard USB-C port supports firmware download and serial debug.
- BOOT is GPIO09.
- U0_RX is GPIO20.
- U0_TX is GPIO21.

## Recommended Flash Method

Use the module onboard USB-C port first. Do not use an external USB-TTL unless the onboard USB-C path fails.

Flash Download Tool settings:

```text
ChipType: ESP32-C3
WorkMode: Developer Mode
LoadMode: UART
SPI SPEED: 40MHz
SPI MODE: DIO
FLASH SIZE: 4MB
BAUD: 115200 first, 460800 only after stable
```

Use the factory image:

```text
0x0  <project root>\92120\<ESP32-C3资料>\ESP32-C3-MINI-1-V2.4.2.0\factory\factory_MINI-1.bin
```

In this workspace, the file is under:

```text
D:\RA6M5\中医四诊\92120\资料\ESP32-C3资料\ESP32-C3-MINI-1-V2.4.2.0\factory\factory_MINI-1.bin
```

If auto-download does not work:

```text
Hold BOOT/FLASH
Click START
Press RST if needed
Release BOOT/FLASH after SYNC/download starts
```

## External USB-TTL Fallback

Only use the UART0 pins from the manual:

```text
USB-TTL TXD -> U0_RX / GPIO20
USB-TTL RXD -> U0_TX / GPIO21
USB-TTL GND -> GND
```

Do not connect to RA6M5 P112/P113 while flashing ESP32-C3 firmware.

## After Flash

Open the module COM port at 115200, 8N1, no flow control.

Send AT with CRLF:

```text
AT\r\n
```

Expected:

```text
OK
```

Then test WiFi:

```text
AT+CWMODE=1
AT+CWLAP
AT+CWJAP="YOUR_WIFI_SSID","YOUR_WIFI_PASSWORD"
```

Only after the module can return `OK` and connect from a serial tool should we reconnect it to RA6M5.
