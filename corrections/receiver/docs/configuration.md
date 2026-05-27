# Configuration Reference

All compile-time configuration is defined in `include/config.h`.

## Hardware Pins (TTGO LoRa32 V2.1)

### LoRa SPI (SX1276)

| Define       | GPIO | Function         |
| ------------ | ---- | ---------------- |
| `LORA_SCK`   | 5    | SPI Clock        |
| `LORA_MISO`  | 19   | SPI MISO         |
| `LORA_MOSI`  | 27   | SPI MOSI         |
| `LORA_SS`    | 18   | SPI Chip Select  |
| `LORA_RESET` | 14   | Radio Reset      |
| `LORA_DIO0`  | 26   | Radio IRQ (DIO0) |

### OLED Display (SSD1306, I2C)

| Define     | GPIO | Function      |
| ---------- | ---- | ------------- |
| `OLED_SDA` | 21   | I2C Data      |
| `OLED_SCL` | 22   | I2C Clock     |
| `OLED_RST` | 16   | Display Reset |

### Other

| Define       | GPIO | Function                    |
| ------------ | ---- | --------------------------- |
| `BUZZER_PIN` | 4    | Buzzer output (active high) |

## LoRa Radio Parameters

| Define                  | Value                 | Description             |
| ----------------------- | --------------------- | ----------------------- |
| `LORA_FREQUENCY`        | 444200000 (444.2 MHz) | Carrier frequency       |
| `LORA_SPREADING_FACTOR` | 7                     | SF7 — fastest data rate |
| `LORA_BANDWIDTH`        | 500E3 (500 kHz)       | Signal bandwidth        |
| `LORA_CODING_RATE`      | 5                     | 4/5 coding rate         |
| `LORA_TX_POWER`         | 20                    | Transmit power in dBm   |
| `LORA_SYNC_WORD`        | 0x12                  | Network sync word       |

Preamble length is set to 12 symbols. Hardware CRC is enabled.

## Protocol Constants

| Define               | Value | Description                     |
| -------------------- | ----- | ------------------------------- |
| `MAX_PACKET_SIZE`    | 512   | Maximum raw packet buffer       |
| `HEADER_SIZE`        | 4     | LoRa packet header size (bytes) |
| `MAX_DATA_SIZE`      | 508   | Max payload per packet          |
| `MAX_MESSAGE_BUFFER` | 16384 | 16 KB reassembly buffer         |

## Timing

| Define                    | Value   | Description                                  |
| ------------------------- | ------- | -------------------------------------------- |
| `DISPLAY_UPDATE_INTERVAL` | 500 ms  | OLED refresh rate                            |
| `PACKET_TIMEOUT`          | 5000 ms | Timeout for incomplete multi-packet messages |
| `BUZZER_PULSE_US`         | 1000 µs | Buzzer pulse duration per beep               |

## WiFi Access Point

| Define                 | Value         | Description                   |
| ---------------------- | ------------- | ----------------------------- |
| `WIFI_SSID`            | `OSS-LoRa-RX` | AP network name               |
| `WIFI_PASSWORD`        | `lora123456`  | AP password (WPA2)            |
| `WIFI_CHANNEL`         | 1             | WiFi channel                  |
| `WIFI_MAX_CONNECTIONS` | 4             | Max simultaneous WiFi clients |

## NTRIP Server

| Define              | Value  | Description                              |
| ------------------- | ------ | ---------------------------------------- |
| `NTRIP_PORT`        | 2101   | TCP listen port                          |
| `NTRIP_MOUNT_POINT` | `LORA` | NTRIP mount point name                   |
| `NTRIP_MAX_CLIENTS` | 4      | Max concurrent NTRIP streams             |
| `RTCM_BUFFER_SIZE`  | 8192   | Circular buffer for RTCM parsing (bytes) |

## Dual-Core (FreeRTOS)

| Define                  | Value | Description                              |
| ----------------------- | ----- | ---------------------------------------- |
| `LORA_CORE`             | 1     | CPU core for LoRa reception              |
| `NETWORK_CORE`          | 0     | CPU core for WiFi/NTRIP/display          |
| `LORA_TASK_STACK`       | 8192  | Stack size for LoRa task (bytes)         |
| `NETWORK_TASK_STACK`    | 8192  | Stack size for network task (bytes)      |
| `LORA_TASK_PRIORITY`    | 2     | FreeRTOS priority (higher = more urgent) |
| `NETWORK_TASK_PRIORITY` | 1     | FreeRTOS priority                        |
| `RTCM_QUEUE_SIZE`       | 16    | Max RTCM messages queued between cores   |

## Debug

| Define                | Value | Description                                 |
| --------------------- | ----- | ------------------------------------------- |
| `ENABLE_UART_LOGGING` | true  | Set to `false` to disable all Serial output |

Serial baud rate: 115200 (configured in `debug.cpp`).

## PlatformIO Build

Target environment: `ttgo-lora32-v21`

Dependencies (from `platformio.ini`):

- `thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays@^4.4.0`
- `sandeepmistry/LoRa@^0.8.0`
