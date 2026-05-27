# System Architecture

## Overview

OSS Receiver is an ESP32-based LoRa-to-NTRIP gateway. It receives RTCM correction data via LoRa radio, reassembles fragmented messages, and serves them to GNSS receivers via a local NTRIP caster over WiFi.

**Hardware**: TTGO LoRa32 V2.1 (ESP32 + SX1276 + SSD1306 OLED)

## Dual-Core Architecture

The firmware runs two FreeRTOS tasks pinned to separate ESP32 cores:

```
┌─────────────────────────────────────────────────────────────────────┐
│                         ESP32 Dual-Core                             │
│                                                                     │
│  Core 1 (loraTask, prio 2)          Core 0 (networkTask, prio 1)   │
│  ─────────────────────────          ────────────────────────────    │
│  LoRa SPI (checkForPackets)         xQueueReceive(rtcmQueue)       │
│  onReceive() → reassembly           broadcastRTCMData() → clients  │
│  processRTCMData()                  handleNTRIPClients()            │
│  parseNextRTCMMessage()             handleWiFiClients()             │
│  buzzerUpdate() (GPIO)              updateDisplay() (I2C/OLED)     │
│  checkMessageTimeout()              removeDisconnectedClients()     │
│           │                                    ▲                    │
│           │        ┌──────────────┐            │                    │
│           └───────►│  rtcmQueue   │────────────┘                    │
│                    │ (FreeRTOS Q) │                                  │
│                    └──────────────┘                                  │
└─────────────────────────────────────────────────────────────────────┘
```

### Core 1 — LoRa Reception (Time-Critical)

Higher priority (2). Handles all radio I/O via SPI:

1. Polls LoRa radio for incoming packets
2. Reassembles multi-packet messages (matching by Message ID)
3. Feeds reassembled data into the RTCM circular buffer
4. Parses complete RTCM frames (preamble, length, CRC validation)
5. Enqueues valid RTCM frames into `rtcmQueue`
6. Drives the buzzer GPIO on each valid frame

### Core 0 — Network & UI

Lower priority (1). Shares core with ESP32 WiFi stack (which also runs on Core 0):

1. Dequeues RTCM frames from `rtcmQueue`
2. Broadcasts them to all authenticated NTRIP clients
3. Accepts new NTRIP client connections
4. Handles NTRIP protocol (GET requests, source table, mount points)
5. Updates OLED display with statistics every 500ms

## Inter-Core Synchronization

| Mechanism    | Type             | Purpose                                            |
| ------------ | ---------------- | -------------------------------------------------- |
| `rtcmQueue`  | FreeRTOS xQueue  | Passes complete RTCM frames from Core 1 → Core 0   |
| `statsMux`   | portMUX spinlock | Protects LoRa statistics read by display on Core 0 |
| `debugMutex` | FreeRTOS mutex   | Serializes Serial output from both cores           |

## Data Flow

```
LoRa Radio (SX1276)
    │ SPI
    ▼
checkForPackets() ──► onReceive()
                         │
                         ▼
                  Packet Reassembly
                  (activeMessages map)
                         │
                         ▼
                  processRTCMData()
                         │
                         ▼
                  RTCM Circular Buffer
                  (addToRTCMBuffer)
                         │
                         ▼
                  parseNextRTCMMessage()
                  (preamble + CRC check)
                         │
                         ▼
                  ┌──────────────┐
                  │  rtcmQueue   │  ◄── Inter-core boundary
                  └──────┬───────┘
                         │
                         ▼
                  broadcastRTCMData()
                         │
                         ▼
                  NTRIP Clients (TCP)
                         │
                         ▼
                  GNSS Rover Receivers
```

## Module Overview

| Module       | File(s)              | Responsibility                                                |
| ------------ | -------------------- | ------------------------------------------------------------- |
| Main         | `main.cpp`           | FreeRTOS task creation, setup sequence                        |
| LoRa Radio   | `lora_radio.cpp/h`   | SPI radio, packet reassembly, RTCM parsing pipeline           |
| RTCM Parser  | `rtcm_parser.cpp/h`  | Circular buffer, RTCM3 frame parsing, CRC-24Q validation      |
| NTRIP Server | `ntrip_server.cpp/h` | TCP server, NTRIP protocol, client management, data broadcast |
| WiFi Manager | `wifi_manager.cpp/h` | Access Point setup, client tracking                           |
| Display      | `display.cpp/h`      | SSD1306 OLED driver, statistics rendering                     |
| Buzzer       | `buzzer.cpp/h`       | Non-blocking GPIO pulse on valid RTCM frame                   |
| Debug        | `debug.cpp/h`        | Thread-safe Serial logging macros                             |
| Config       | `config.h`           | All compile-time constants (pins, radio, network, timing)     |

## Boot Sequence

```
setup()
  ├── initDebug()           — Serial + debug mutex
  ├── setupOLED()           — I2C display init
  ├── showSplashScreen()    — 3s splash
  ├── setupWiFiHotspot()    — AP mode
  ├── setupNTRIPServer()    — TCP listener on port 2101
  ├── initRTCMBuffer()      — Circular buffer reset
  ├── setupBuzzer()         — GPIO init
  ├── setupLoRa()           — SPI + radio config
  ├── xQueueCreate()        — Inter-core queue
  ├── xTaskCreatePinnedToCore(loraTask, Core 1)
  └── xTaskCreatePinnedToCore(networkTask, Core 0)

loop() → vTaskDelete(NULL)  — Arduino loop unused
```
