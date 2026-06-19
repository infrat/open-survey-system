# OSS RTCM Transmitter

ESP32 + SX1276 RTK gateway connecting NTRIP clients to LoRa transmitters for RTCM corrections.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![Status](https://img.shields.io/badge/status-active-success.svg)

## Overview

Open Source Surveying Transmitter is an ESP32 firmware that transforms the device into a professional RTK gateway. The system receives GNSS corrections from NTRIP servers over WiFi and retransmits them over long distances via LoRa, enabling precise RTK positioning for rovers without internet access.

### Key Features

- **NTRIP Client** - Fetches RTCM3 corrections from NTRIP casters
- **LoRa Transmitter** - 433 MHz transmission with fragmentation protocol
- **RTCM Parser** - CRC validation and message type filtering
- **Web Interface** - Browser-based configuration (AP mode)
- **OLED Display** - Real-time status and statistics
- **ConfigManager** - Persistent configuration in LittleFS
- **Range Optimization** - Frame skipping for bandwidth savings
- **State Machine** - Synchronized TX/SILENCE cycles

## Use Cases

- **RTK Surveying** - Base station for multi-point surveys when internet is unavailable in the field
- **Precision Agriculture** - RTK guidance for autonomous machinery

## Hardware

### Required

- **TTGO T-Beam V1.1 or LoRa32 or similar** (ESP32 + SX1276 + OLED)
- **433 MHz LoRa Antenna** (recommended: dipole/yagi for range)
- **Power Supply** - USB-C or 18650 Li-Ion (3.7V)

### SX1276 Pinout (T-Beam predefined)

```
NSS   → GPIO 18
MOSI  → GPIO 27
MISO  → GPIO 19
SCK   → GPIO 5
RST   → GPIO 23
DIO0  → GPIO 26
```

### OLED Display

- **Type**: SSD1306 128x64
- **I2C**: GPIO 21 (SDA), GPIO 22 (SCL)

## Quick Start

### 1. Environment Setup

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone <repo-url>
cd oss-transmitter
```

### 2. Basic Configuration

All configuration is now managed via the web interface or by editing the configuration file stored in LittleFS.

On first boot, the device creates a WiFi access point:

- SSID: `OSS-LoRa-TX-XXXXXX` (where XXXXXX is chip ID)
- Password: (no password)

Connect to this AP and configure:

- WiFi credentials
- NTRIP server settings
- LoRa parameters
- GGA position (for VRS)

Alternatively, edit [include/config.h](include/config.h) for default values:

```cpp
// NTRIP Caster
#define NTRIP_HOST "91.198.76.2"
#define NTRIP_PORT 2101
#define NTRIP_MOUNTPOINT "RTN4G_VRS_RTCM32"

// GGA Position (for VRS)
#define GGA_LATITUDE 50.2346     // Your latitude
#define GGA_LONGITUDE 19.2084    // Your longitude
#define GGA_ALTITUDE 266.0       // Altitude MSL

// LoRa Radio
#define LORA_FREQUENCY 433E6
#define LORA_SPREADING_FACTOR 7
#define LORA_TX_POWER 20
```

### 3. Build and Upload

```bash
# Build firmware
platformio run --environment ttgo-t-beam

# Upload to device
platformio run --target upload --environment ttgo-t-beam

# Monitor serial output
platformio device monitor --baud 115200
```

## LoRa Protocol

### Packet Structure

```
┌────────────┬───────────────┬──────────────┬─────────────┬──────────────────┐
│ Message ID │ Total Packets │ Packet Index │ Data Length │      Data        │
│  (1 byte)  │   (1 byte)    │   (1 byte)   │  (1 byte)   │  (0-246 bytes)   │
└────────────┴───────────────┴──────────────┴─────────────┴──────────────────┘
```

### Transmission Parameters

- **Frequency**: 433 MHz (configurable)
- **Spreading Factor**: 7 (speed/range compromise)
- **Bandwidth**: 125 kHz
- **Coding Rate**: 4/5
- **TX Power**: 20 dBm (max for SX1276)
- **Sync Word**: 0x12
- **CRC**: Hardware CRC enabled

### Fragmentation

Large RTCM messages are automatically fragmented:

- Max packet size: **250 bytes**
- Header size: **4 bytes**
- Max data per packet: **246 bytes**
- Max message size: **~62.7 KB** (255 × 246)

**Example**: RTCM message 1077 (500 bytes) → 3 LoRa packets

See [lora-protocol.md](../shared/docs/lora-protocol.md) for the complete (shared) specification.

## System Architecture

### State Machine

```
[INIT] → [WAITING_FOR_FIRST_RTCM] → [TRANSMISSION] ⇄ [GATHERING]
                                              ↓
                                         [ERROR]
```

1. **STATE_INIT** - Initialize WiFi, NTRIP, LoRa
2. **STATE_WAITING_FOR_FIRST_RTCM** - Wait for first data
3. **STATE_TRANSMISSION** (3.5s) - Transmit all buffered RTCM
4. **STATE_GATHERING** (0.25s) - Radio silence, buffer new RTCM
5. **STATE_ERROR** - Handle connection errors

### Main Components

| Module              | File                                                       | Responsibility                    |
| ------------------- | ---------------------------------------------------------- | --------------------------------- |
| **ConfigManager**   | [config_manager.cpp](src/config_manager.cpp)               | LittleFS storage, web API         |
| **WiFiManager**     | [wifi_manager.cpp](src/wifi_manager.cpp)                   | WiFi client + AP mode             |
| **NTRIPClient**     | [ntrip_client.cpp](src/ntrip_client.cpp)                   | TCP client, GGA transmission      |
| **RTCMParser**      | [rtcm_parser.cpp](src/rtcm_parser.cpp)                     | RTCM3 parsing, CRC validation     |
| **RTCMBuffer**      | [rtcm_buffer.h](include/rtcm_buffer.h)                     | Type-based buffering and grouping |
| **LoRaTransmitter** | [lora_transmitter.cpp](src/lora_transmitter.cpp)           | Fragmentation and transmission    |
| **DisplayManager**  | [display_manager.cpp](src/display_manager.cpp)             | OLED status screen                |
| **WebInterface**    | [web_interface_manager.cpp](src/web_interface_manager.cpp) | AsyncWebServer + REST API         |

### Timing

- **TX Burst**: 3500 ms (transmit all collected RTCM)
- **Silence Period**: 250 ms (buffer only, no TX)
- **GGA Interval**: 2000 ms (VRS position update)
- **Display Update**: 500 ms
- **Serial Stats**: 5000 ms

## RTCM Message Types

### Default Transmitted Types

```cpp
// GPS RTK Observables
1001, 1002, 1003, 1004

// GPS/GLONASS Ephemeris
1019, 1020

// GPS SSR Corrections
1057, 1058, 1059, 1060, 1061, 1062

// GLONASS RTK
1009, 1010, 1011, 1012

// MSM (Multiple Signal Messages)
1074, 1075, 1076, 1077  // GPS L1/L2
1084, 1085, 1086, 1087  // GLONASS
1094, 1095, 1096, 1097  // Galileo

// Reference Station Info
1005, 1006, 1007, 1008, 1033
```

### Why Filtering?

Due to the limited bandwidth of the LoRa radio link, RTCM messages coming from the NTRIP server must be filtered. The current configuration supports the minimum set of messages necessary to achieve a stable RTK fix, balancing data completeness with transmission constraints.

Filtering is configurable via **Web Interface** → **RTCM Filtering**.

## Debugging

### Enable DEBUG Mode

In [include/config.h](include/config.h):

```cpp
#define DEBUG                    // General debug output
#define DEBUG_LORA_PACKETS true  // Detailed packet logs
#define DEBUG_LORA_VERBOSE true  // Ultra-verbose logging
```

### Common Issues

| Problem                   | Solution                                                     |
| ------------------------- | ------------------------------------------------------------ |
| **WiFi won't connect**    | Check credentials in web interface, increase timeout to 60s  |
| **NTRIP error 401**       | Wrong credentials, verify username/password in web interface |
| **No RTCM data**          | Wrong GGA position (out of VRS range) or mountpoint          |
| **LoRa not transmitting** | Check antenna, SWR, power settings                           |
| **Restart every minute**  | Watchdog - possible deadlock in NTRIP client                 |

## Contributing

Open source project - contributions welcome!

1. Fork the repository
2. Create a branch (`git checkout -b feature/amazing-feature`)
3. Commit changes (`git commit -m 'Add amazing feature'`)
4. Push to branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

**Important**: Read [AGENTS.md](AGENTS.md) before starting work - it contains coding standards and workflow for this project.

## Technical Documentation

### Shared specs (transmitter ↔ receiver)

These live in `firmware/shared/docs/` because both firmwares must stay byte-compatible with them:

- [lora-protocol.md](../shared/docs/lora-protocol.md) - LoRa over-the-air packet protocol
- [maintenance-frame.md](../shared/docs/maintenance-frame.md) - Maintenance/telemetry and command frame protocol

### Transmitter-specific

- [docs/config-structure.md](docs/config-structure.md) - Web UI configuration structure and REST API contract
- [AGENTS.md](AGENTS.md) - Project standards and workflow for AI agents

## License

MIT License - free to use in private and commercial projects.

## Acknowledgments

- **ThingPulse** - ESP8266/ESP32 OLED driver
- **Sandeep Mistry** - Arduino LoRa library
- **me-no-dev** - ESPAsyncWebServer
- **bblanchon** - ArduinoJson

## Contact

Bug reports: [GitHub Issues](https://github.com/yourusername/oss-transmitter/issues)

---

**Project Status**: Active Development
