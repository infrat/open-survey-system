# Open Survey System (OSS) - RTK Rover Firmware

**ESP32 Firmware for GNSS RTK Rover**

This is the main ESP32 firmware for the **Open Survey System (OSS)** - an open-source GNSS RTK rover designed for professional surveying and mapping applications. The firmware provides a bidirectional **UART ↔ Bluetooth bridge** to connect the GNSS RTK receiver with iOS/Android devices, over either **BLE** (Nordic UART Service, default) or **Bluetooth Classic SPP** (Android only) - see [Transport Selection](#transport-selection-ble-or-bluetooth-spp).

## Project Overview

The **Open Survey System RTK Rover** consists of three main components:

1. **GNSS RTK Receiver**: Quectel LC29H DA (dual-antenna, multi-band RTK)
2. **ESP32 MCU**: Main controller running this firmware (Wemos D1 Mini32)
3. **Power Management Module**: Battery management and power distribution

This firmware handles:

- ✅ Real-time NMEA/RTCM data streaming via BLE Nordic UART Service (NUS) or Bluetooth Classic SPP
- ✅ Bidirectional communication (corrections from mobile → GNSS)
- ✅ Over-The-Air (OTA) firmware updates via WiFi
- ✅ Power optimization for extended battery life
- ✅ Status LED indication

## Transport Selection (BLE or Bluetooth SPP)

The mobile-facing side of the bridge is chosen **at compile time** via `ROVER_TRANSPORT`. Only one Bluetooth host stack can run at a time (NimBLE and Bluedroid are mutually exclusive), so this is a build variant, not a runtime switch.

| Transport                 | PlatformIO env  | Stack     | iOS | Android | Flash   | Notes                           |
| ------------------------- | --------------- | --------- | --- | ------- | ------- | ------------------------------- |
| `TRANSPORT_BLE` (default) | `oss-rover`     | NimBLE    | ✅  | ✅      | ~1.08MB | Nordic UART Service, 185B MTU   |
| `TRANSPORT_SPP`           | `oss-rover-spp` | Bluedroid | ❌  | ✅      | ~1.59MB | Plain byte stream, no MTU limit |

```bash
pio run -e oss-rover          # BLE build (default)
pio run -e oss-rover-spp      # Bluetooth Classic SPP build
```

The default lives in `include/config.h` and each env overrides it with a build flag:

```cpp
#define TRANSPORT_BLE 0
#define TRANSPORT_SPP 1

#ifndef ROVER_TRANSPORT
#define ROVER_TRANSPORT TRANSPORT_BLE
#endif
```

```ini
build_flags = -DROVER_TRANSPORT=TRANSPORT_SPP
```

**⚠️ iOS cannot use SPP.** Bluetooth Classic SPP is not available to third-party iOS apps without MFi certification. Keep the BLE build for any iPhone/iPad workflow.

**Trade-offs of the SPP build:**

- **Throughput**: higher - no 185-byte MTU and no 10ms inter-chunk delay; `BluetoothSerial` queues the payload and fragments it in its own TX task
- **Power**: worse - Bluetooth Classic keeps a far more expensive link up than BLE at -9dBm, so the ~20-30h battery estimate no longer holds
- **Flash/RAM**: Bluedroid + SPP costs roughly +500KB flash over NimBLE (80.7% vs 55.2% of the 1.92MB app partition) and more runtime heap - watch `status` over Telnet for free heap
- **Pairing**: SPP requires the phone to pair with `OSSRTK` first, then open the serial port; BLE just connects

Adding a new transport means implementing `ITransport` (see `src/transport.h`) and adding a branch to `src/rover_transport.h` - `main.cpp` stays untouched.

## Hardware Components

## UART Pinout (ESP32 ↔ LC29H DA)

```
LC29H DA (GNSS)     ESP32 (Wemos D1 Mini32)
---------------     -----------------------
VCC_IN (3.3-5V) --> 3.3V (via power module)
GND             --> GND
UART1_TX        --> GPIO25 (RX - GNSS data to ESP32)
UART1_RX        <-- GPIO27 (TX - corrections to GNSS)
```

**UART Configuration:**

- Baud Rate: 115200
- Data: 8N1 (8 bits, no parity, 1 stop bit)
- Flow Control: None

## Power Optimization Features

The firmware implements aggressive power-saving to maximize battery life:

### Dynamic CPU Frequency Scaling

- **Idle Mode**: 80MHz when no data transfer (~30mA saved)
- **Active Mode**: 240MHz during GNSS data streaming
- **Auto-switching**: 100ms threshold

### WiFi Auto-Shutdown

- WiFi **active for 3 minutes** after boot (for OTA updates)
- **Auto-disable** after timeout (~100mA saved)
- Total WiFi-off power: **~105mA** (down from 250mA)

### Bluetooth Low Power Mode

- TX Power: **-9dBm** (`BLE_TX_POWER` / `SPP_TX_POWER`, reduced from +9dBm)
- Range: ~10-15m (sufficient for rover-to-mobile)
- Power saving: **~15mA** on the BLE build
- The SPP build applies the same TX level via `esp_bredr_tx_power_set()`, but Bluetooth Classic still draws noticeably more than BLE

### Total Power Savings

- **Active (data streaming)**: ~250mA
- **Idle (WiFi off, low BLE)**: ~105mA
- **Battery life**: ~20-30 hours (with 3000mAh battery)

## Mobile App Compatibility

### Recommended iOS Apps (BLE build only)

1. **Lefebure NTRIP Client** - Professional RTK/NTRIP app
2. **SW Maps** - Survey and mapping with NTRIP support
3. **nRF Toolbox** (Nordic) - Testing NUS connection

### Android Apps (both builds)

1. **Lefebure NTRIP Client**
2. **Mobile Topographer**
3. **Serial Bluetooth Terminal** - works against the SPP build (pair first) and against the BLE build in BLE mode

## Building and Uploading

### First Upload (via USB)

```bash
# Build firmware (BLE default; use -e oss-rover-spp for the SPP variant)
pio run -e oss-rover

# Upload via USB
pio run -e oss-rover --target upload

# Monitor serial output
pio device monitor
```

### OTA Updates (via WiFi)

After initial USB upload, you can update wirelessly:

```bash
# Ensure ESP32 is powered on and WiFi is active (first 3 minutes)
pio run -t upload

# Firmware will be sent to: ossrtk.local
# OTA password: admin (configurable in config.h)
```

**Important**: WiFi is only active for **3 minutes after boot** to save power. For OTA updates:

1. Reboot the rover
2. Wait for WiFi connection (check serial logs or LED)
3. Upload within 3 minutes
4. WiFi auto-disables after timeout

## Configuration

Edit `include/config.h` to customize:

### UART Settings (GNSS)

```cppe GPS_RX_PIN 25           // GNSS TX → ESP32 RX
#define GPS_TX_PIN 27           // ESP32 TX → GNSS RX
#define GPS_BAUD_RATE 115200    // LC29H DA baud rate
#define UART_BUF_SIZE 1024      // UART buffer (1KB)
```

### WiFi/OTA Settings

```cpp
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#define OTA_HOSTNAME "ossrtk"          // ossrtk.local
#define OTA_PASSWORD "admin"
#define WIFI_ACTIVE_TIME_MS 180000     // 3 minutes
```

### Power Saving

```cpp
#define BLE_TX_POWER ESP_PWR_LVL_N9    // -9dBm (low power, BLE build)
#define SPP_TX_POWER ESP_PWR_LVL_N9    // -9dBm (low power, SPP build)
#define CPU_FREQ_ACTIVE 240            // MHz when streaming
```

### Bluetooth Settings

```cpp
#define BT_DEVICE_NAME "OSSRTK"        // BLE scanner name / BT pairing name

// BLE build only:
#define BLE_MTU_SIZE 185               // iOS max MTU
#define BLE_CHUNK_SIZE 180             // Chunk size for large packets
```

### Status LED

```cpp
#define LINK_STATUS_LED_PIN 23         // GPIO23 (active HIGH)
#define LINK_LED_BLINK_MS 250          // Blink period when disconnected
```

## Nordic UART Service (NUS) UUIDs

The firmware implements the standard Nordic UART Service for maximum compatibility:

```
Service UUID:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
RX Char UUID:  6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (Mobile → ESP32 → GNSS)
TX Char UUID:  6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (GNSS → ESP32 → Mobile)

## Debugging and Monitoring

### Serial Monitor (USB - 115200 baud)

```

========================================
ESP32 UART-BLE Bridge
GPS (NMEA) ↔ iOS via Nordic UART Service
========================================

[Setup] Connecting to WiFi...
[WiFi] Connected!
[WiFi] IP Address: 192.168.1.100
[WiFi] Will auto-disable after 180 seconds
[OTA] Ready for firmware updates
[OTA] Hostname: ossrtk.local
[Setup] Initializing UART...
[UART] Initialized GPS UART
[UART] RX: GPIO25, TX: GPIO27, Baud: 115200
[Setup] Initializing BLE...
[BLE] Nordic UART Service started
[BLE] TX Power set to -12dBm (low power mode)
[BLE] Device name: OSSRTK
[Power] CPU frequency: 80 MHz (idle mode)
[Setup] Initialization complete!

Waiting for GPS data and iOS connection...

```

```

**Data Flow:**

- **TX (ESP32 → Mobile)**: NMEA sentences, RTCM corrections status
- **RX (Mobile → ESP32)**: RTCM corrections from NTRIP caster

### Status LED Patterns

| Pattern                   | Status                                  |
| ------------------------- | --------------------------------------- |
| **Blinking (250ms)**      | Waiting for a mobile connection         |
| **Solid ON**              | Mobile device connected                 |
| **Fast blinking (100ms)** | Error - UART init failed                |
| **Fast blinking (200ms)** | Error - transport (BLE/SPP) init failed |

## Troubleshooting

### No GNSS Data Received

**Check UART connection:**

```
LC29H DA TX (pin 5) → ESP32 GPIO25 (RX)
LC29H DA RX (pin 4) → ESP32 GPIO27 (TX)
```

**Verify in Serial Monitor:**

- Look for `[UART] Initialized GPS UART`
- Should see `[Bridge] UART → BLE: XX bytes` when GNSS is outputting data

**LC29H DA Configuration:**

- Default baud: 115200 (some modules ship at 9600 - check datasheet)
- Output rate: 1Hz or higher
- NMEA messages: GGA, RMC, GSA, GSV (minimum)

### Mobile App Can't Connect

**Verify the transport is running:**

- BLE build: serial log shows `[BLE] Nordic UART Service started`
- SPP build: serial log shows `[SPP] Serial Port Profile server started`
- Device name: `OSSRTK`
- LED is blinking (waiting for connection)

**On mobile device (BLE build):**

1. Enable Bluetooth
2. Scan for `OSSRTK`
3. Connect (should see `[BLE] iOS device connected!` in logs)
4. LED should become solid ON

**On mobile device (SPP build, Android):**

1. Pair with `OSSRTK` in the Android Bluetooth settings first
2. Open the serial port from the app (should see `[SPP] Client connected!` in logs)
3. LED should become solid ON

If an iPhone cannot see or connect to the rover, check which variant is flashed - the SPP build is invisible to iOS apps.

**Use BLE Scanner app** to verify device is advertising with correct UUIDs

### No RTK Fix / Low Accuracy

**This is NOT a firmware issue** - check GNSS configuration:

- GNSS has clear sky view (no multipath/obstructions)
- RTCM corrections are being received (check mobile app)
- Corrections match GNSS constellation (GPS/GLO/GAL/BDS)
- Base station <40km away for RTK
- Wait 30-120s for RTK convergence

### OTA Update Failed

**WiFi only active for 3 minutes after boot:**

1. Power cycle the rover
2. Watch serial logs for WiFi connection
3. Run `pio run -t upload` within 3 minutes
4. Check `ossrtk.local` is reachable: `ping ossrtk.local`

**If OTA still fails:**

- Verify WiFi credentials in `config.h`
- Check firewall isn't blocking mDNS/port 3232
- Use IP address instead: `upload_port = 192.168.1.100`

### High Power Consumption

**Expected power draw:**

- **With WiFi ON**: ~250mA (first 3 minutes only)
- **Idle (no data)**: ~105mA
- **Active streaming**: ~140-180mA

**If higher than expected:**

- Check WiFi auto-disabled after 3 min (`[WiFi] Auto-shutdown` log)
- Verify CPU frequency scaling (`[Power] CPU frequency: 80 MHz`)
- Ensure BLE TX power is -12dBm (`[BLE] TX Power set to -12dBm`)

## Example NMEA/RTCM Data

### NMEA Output (LC29H DA → Mobile)

```
$GNGGA,083559.00,5015.12345,N,01945.67890,E,4,24,0.6,123.4,M,45.2,M,1.2,0138*6F
$GNRMC,083559.00,A,5015.12345,N,01945.67890,E,0.012,,101224,,,D*7A
$GNGSA,A,3,01,03,06,09,14,17,19,22,,,,,1.2,0.6,1.0*3C
```

### RTCM Corrections (Mobile → LC29H DA)

```
Binary RTCM3 messages (e.g., 1005, 1077, 1087, 1097, 1127)
Sent from NTRIP caster via mobile app through BLE
```

## Project Structure

```
firmware/rover/
├── include/
│   └── config.h                 # Main configuration (transport, pins, WiFi, power)
├── src/
│   ├── main.cpp                 # Main firmware logic (transport-agnostic)
│   ├── uart_handler.cpp/h       # UART communication with LC29H DA
│   ├── transport.h              # ITransport interface (no Bluetooth includes)
│   ├── rover_transport.h        # Compile-time transport selection
│   ├── ble_uart_service.cpp/h   # BLE NUS implementation (NimBLE)
│   └── spp_serial_service.cpp/h # Bluetooth Classic SPP implementation
├── platformio.ini               # Standalone PlatformIO configuration
└── README.md                    # This file
```

Both implementations are compiled from the same source tree; the unused one is guarded out with `#if ROVER_TRANSPORT == ...` so it becomes an empty translation unit.

## Dependencies

- **Platform**: Espressif32 (PlatformIO)
- **Framework**: Arduino
- **Libraries**:
  - `NimBLE-Arduino` v1.4.0+ (h2zero) - Lightweight BLE stack (BLE build only)
  - `BluetoothSerial` - Framework-bundled Bluedroid SPP wrapper (SPP build only)
  - Built-in: WiFi, ArduinoOTA, esp_bt

## Technical Specifications

| Parameter           | Value                                         |
| ------------------- | --------------------------------------------- |
| **UART Speed**      | 115200 baud                                   |
| **Transport**       | BLE NUS (default) or BT Classic SPP           |
| **BLE Range**       | ~10-15m (-9dBm TX power)                      |
| **BLE MTU**         | 185 bytes (iOS max)                           |
| **Data Chunk Size** | 180 bytes (BLE packet); SPP streams unchunked |
| **WiFi OTA Window** | 3 minutes after boot                          |
| **CPU Frequency**   | 80MHz idle / 240MHz active                    |
| **Power (idle)**    | ~105mA @ 3.3V                                 |
| **Power (active)**  | ~140-180mA @ 3.3V                             |
| **Flash Usage**     | ~1.0MB / 4MB (25%)                            |
| **RAM Usage**       | ~59KB / 320KB (18%)                           |

## Contributing

This is part of the **Open Survey System** project. Contributions welcome!

- Report issues on GitHub
- Submit pull requests for improvements
- Share your rover builds and field tests

## License

MIT License - Free for personal and commercial use

## Acknowledgments

- **Nordic Semiconductor** - Nordic UART Service specification
- **h2zero** - NimBLE-Arduino lightweight BLE library
- **Espressif Systems** - ESP32 platform
- **Quectel** - LC29H DA GNSS module documentation
- **Open Survey System Community** - Testing and feedback
