# OSS RTCM Receiver

ESP32 + SX1276 LoRa → NTRIP gateway that turns long-range RTCM corrections into a local caster for GNSS rovers.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![Status](https://img.shields.io/badge/status-active-success.svg)

## Overview

Open Source Surveying Receiver is an ESP32 firmware that acts as the field-side counterpart of the [OSS Transmitter](../transmitter/README.md). It receives RTCM3 corrections over a 433.7 MHz LoRa link, reassembles fragmented messages, validates them, and re-serves them to nearby GNSS rovers through a local NTRIP caster over its own WiFi access point — no internet connection required in the field.

### Key Features

- **LoRa Receiver** - 433.7 MHz reception with fragment reassembly
- **RTCM Parser** - RTCM3 frame parsing with CRC-24Q validation
- **NTRIP Caster** - Local TCP caster serving the `LORA` mount point
- **WiFi Access Point** - Self-hosted hotspot for rovers (no internet needed)
- **OLED Display** - Real-time RX statistics and client count
- **Buzzer Feedback** - Audible pulse on every valid RTCM frame
- **Dual-Core Design** - Time-critical radio isolated from network/UI work

## Use Cases

- **RTK Surveying** - Deliver base corrections to rovers where cellular/internet is unavailable
- **Precision Agriculture** - Local correction relay for autonomous machinery

## Hardware

### Required

- **TTGO LoRa32 V2.1** (ESP32 + SX1276 + SSD1306 OLED)
- **433/444 MHz LoRa Antenna** (recommended: dipole/yagi for range)
- **Power Supply** - USB-C or 18650 Li-Ion (3.7V)

### SX1276 Pinout (TTGO LoRa32 V2.1)

```
NSS   → GPIO 18
MOSI  → GPIO 27
MISO  → GPIO 19
SCK   → GPIO 5
RST   → GPIO 14
DIO0  → GPIO 26
```

### OLED Display

- **Type**: SSD1306 128x64
- **I2C**: GPIO 21 (SDA), GPIO 22 (SCL), GPIO 16 (RST)

### Buzzer

- **Pin**: GPIO 4 (non-blocking pulse on valid RTCM frame)

## Quick Start

### 1. Environment Setup

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone <repo-url>
cd gnss-rtk-toolset/firmware/receiver
```

### 2. Basic Configuration

All compile-time settings live in [include/config.h](include/config.h). The defaults work out of the box for a paired transmitter:

```cpp
// WiFi Access Point
#define WIFI_SSID "OSS-LoRa-RX"
#define WIFI_PASSWORD "lora123456"   // change before any real deployment

// NTRIP Caster
#define NTRIP_PORT 2101
#define NTRIP_MOUNT_POINT "LORA"
#define NTRIP_MAX_CLIENTS 4

// LoRa Radio (must match the transmitter)
#define LORA_FREQUENCY 433700000
#define LORA_SPREADING_FACTOR 7
#define LORA_BANDWIDTH 500E3
#define LORA_TX_POWER 20
```

> ⚠️ The LoRa radio parameters must be identical to the transmitter or the link fails silently.

### 3. Build and Upload

```bash
# Build firmware
pio run -e ttgo-lora32-v21

# Upload to device
pio run -e ttgo-lora32-v21 -t upload

# Monitor serial output
pio device monitor --baud 115200
```

### 4. Connect a Rover

1. On the rover/phone, join the WiFi network `OSS-LoRa-RX` (password `lora123456`).
2. Point an NTRIP client at the caster:
   - **Host**: `192.168.4.1`
   - **Port**: `2101`
   - **Mount point**: `LORA`
   - **Auth**: none
3. The rover starts receiving RTCM and converges to an RTK fix.

## LoRa Protocol

The receiver stays byte-compatible with the transmitter's packet format.

### Packet Structure (Protocol v2)

```
┌────────────┬────────────┬───────────────┬──────────────┬─────────────┬──────────────────┐
│ Frame Type │ Message ID │ Total Packets │ Packet Index │ Data Length │      Data        │
│  (1 byte)  │  (1 byte)  │   (1 byte)    │   (1 byte)   │  (1 byte)   │  (0-245 bytes)   │
└────────────┴────────────┴───────────────┴──────────────┴─────────────┴──────────────────┘
```

| Frame Type          | Value  | Direction        | Purpose              |
| ------------------- | ------ | ---------------- | -------------------- |
| `FRAME_RTCM`        | `0x01` | TX → air         | RTCM correction data |
| `FRAME_MAINTENANCE` | `0x02` | TX → air         | Telemetry broadcast  |
| `FRAME_COMMAND`     | `0x03` | RX → transmitter | Control command      |
| `FRAME_ACK`         | `0x04` | optional         | Acknowledgement      |

### Reception Parameters

- **Frequency**: 433.7 MHz (`433700000`)
- **Spreading Factor**: 7
- **Bandwidth**: 500 kHz
- **Coding Rate**: 4/5
- **Sync Word**: 0x12
- **Header size**: 5 bytes
- **Max data per packet**: 245 bytes
- **Reassembly buffer**: 16 KB

Incomplete multi-packet messages are dropped after `PACKET_TIMEOUT` (5 s). See [docs/lora-protocol.md](docs/lora-protocol.md) and the shared spec in [../shared/docs/lora-protocol.md](../shared/docs/lora-protocol.md) for the full specification.

## System Architecture

### Dual-Core Design

The firmware runs two FreeRTOS tasks pinned to separate cores so radio reception is never starved by network or UI work.

```
Core 1 (loraTask, prio 2)            Core 0 (networkTask, prio 1)
─────────────────────────            ────────────────────────────
checkForPackets()  (SPI)             xQueueReceive(rtcmQueue)
onReceive() → reassembly             broadcastRTCMData() → clients
processRTCMData()                    handleNTRIPClients()
parseNextRTCMMessage()               handleWiFiClients()
buzzerUpdate() (GPIO)                updateDisplay() (I2C/OLED)
          │                                     ▲
          └──────────► rtcmQueue ───────────────┘
                       (FreeRTOS Q)
```

- **Core 1 — LoRa Reception (priority 2)**: radio SPI I/O, packet reassembly, RTCM parsing/CRC, buzzer.
- **Core 0 — Network & UI (priority 1)**: NTRIP broadcast, client management, OLED refresh.

### Data Flow

```
LoRa Radio (SX1276) → checkForPackets() → onReceive()
  → packet reassembly (activeMessages map)
  → processRTCMData() → RTCM circular buffer
  → parseNextRTCMMessage() (preamble + CRC-24Q)
  → rtcmQueue  [INTER-CORE BOUNDARY]
  → broadcastRTCMData() → NTRIP clients (TCP) → GNSS rovers
```

### Inter-Core Synchronization

| Mechanism    | Type             | Purpose                                            |
| ------------ | ---------------- | -------------------------------------------------- |
| `rtcmQueue`  | FreeRTOS xQueue  | Passes complete RTCM frames from Core 1 → Core 0   |
| `statsMux`   | portMUX spinlock | Protects LoRa statistics read by display on Core 0 |
| `debugMutex` | FreeRTOS mutex   | Serializes Serial output from both cores           |

### Main Components

| Module           | File                                         | Responsibility                                 |
| ---------------- | -------------------------------------------- | ---------------------------------------------- |
| **Main**         | [src/main.cpp](src/main.cpp)                 | FreeRTOS task creation, boot sequence          |
| **LoRa Radio**   | [src/lora_radio.cpp](src/lora_radio.cpp)     | SPI radio, packet reassembly, parsing pipeline |
| **RTCM Parser**  | [src/rtcm_parser.cpp](src/rtcm_parser.cpp)   | Circular buffer, RTCM3 parsing, CRC-24Q        |
| **NTRIP Server** | [src/ntrip_server.cpp](src/ntrip_server.cpp) | TCP caster, client management, broadcast       |
| **WiFi Manager** | [src/wifi_manager.cpp](src/wifi_manager.cpp) | Access Point setup, client tracking            |
| **Display**      | [src/display.cpp](src/display.cpp)           | SSD1306 OLED driver, statistics rendering      |
| **Buzzer**       | [src/buzzer.cpp](src/buzzer.cpp)             | Non-blocking GPIO pulse on valid RTCM frame    |
| **Debug**        | [src/debug.cpp](src/debug.cpp)               | Thread-safe Serial logging macros              |
| **Config**       | [include/config.h](include/config.h)         | All compile-time constants                     |

### Timing

- **Display Update**: 500 ms
- **Packet Timeout**: 5000 ms (drop incomplete multi-packet messages)

## NTRIP Server

- Caster listens on TCP port `2101`, mount point `LORA`, max 4 clients.
- `GET / HTTP/1.1` → source table, then close.
- `GET /LORA HTTP/1.1` → `ICY 200 OK` + binary RTCM stream (connection stays open).
- No free client slot → `503 Service Unavailable` → close.

See [docs/ntrip-server.md](docs/ntrip-server.md) for the full protocol flow and rover configuration.

## Debugging

### Serial Output

Serial baud is `115200`. Both cores log through thread-safe macros that take `debugMutex`:

```cpp
DEBUG_PRINTLN(F(">>> LoRa init..."));      // status
DEBUG_PRINTLN(F("[NTRIP] client added"));  // module-tagged event
DEBUG_PRINTLN(F("[ERROR] CRC failed"));    // error
```

Set `ENABLE_UART_LOGGING false` in [include/config.h](include/config.h) to silence all output.

### Common Issues

| Problem                        | Solution                                                             |
| ------------------------------ | -------------------------------------------------------------------- |
| **No LoRa frames received**    | Check antenna and verify radio params match the transmitter exactly  |
| **Frames received, CRC fails** | Spreading factor / bandwidth / sync word mismatch with transmitter   |
| **Rover can't connect WiFi**   | Confirm SSID `OSS-LoRa-RX` and password in `config.h`                |
| **NTRIP client gets 503**      | All 4 client slots in use — disconnect an idle client                |
| **No RTK fix on rover**        | Out of base range, obstructed sky view, or wait 30-120 s to converge |
| **Watchdog resets**            | A task blocked too long — keep the LoRa core lean, yield in loops    |

## Contributing

Open source project - contributions welcome!

1. Fork the repository
2. Create a branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

**Important**: Read [AGENTS.md](AGENTS.md) before starting work - it contains coding standards and the dual-core workflow for this project.

## Technical Documentation

### Shared specs (transmitter ↔ receiver)

These live in `firmware/shared/docs/` because both firmwares must stay byte-compatible with them:

- [lora-protocol.md](../shared/docs/lora-protocol.md) - LoRa over-the-air packet protocol
- [maintenance-frame.md](../shared/docs/maintenance-frame.md) - Maintenance/telemetry and command frame protocol

### Receiver-specific

- [docs/architecture.md](docs/architecture.md) - Dual-core task/core structure
- [docs/configuration.md](docs/configuration.md) - Pins, timing, and tunable constants
- [docs/lora-protocol.md](docs/lora-protocol.md) - Receiver view of the radio link
- [docs/ntrip-server.md](docs/ntrip-server.md) - NTRIP caster behavior and rover setup
- [AGENTS.md](AGENTS.md) - Project standards and workflow for AI agents

## License

MIT License - free to use in private and commercial projects.

## Acknowledgments

- **ThingPulse** - ESP8266/ESP32 OLED driver
- **Sandeep Mistry** - Arduino LoRa library

## Contact

Bug reports: [GitHub Issues](https://github.com/yourusername/gnss-rtk-toolset/issues)

---

**Project Status**: Active Development
