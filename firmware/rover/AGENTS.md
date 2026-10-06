# AGENTS.md - OSS Rover Project Standards

Guidelines for AI agents (GitHub Copilot, Cursor, Claude, etc.) and developers working on this project.

## 🎯 Project Philosophy

**OSS Rover** is professional embedded firmware for the ESP32. It is a bidirectional **UART ↔ wireless bridge** that connects a Quectel **LC29H DA** GNSS RTK receiver to iOS/Android survey apps. One firmware image offers three client links, chosen at runtime in the web UI: the **Nordic UART Service (NUS)** on BLE (default), **Bluetooth Classic SPP** (Android only) or a raw **WiFi TCP** socket - see [Transport Abstraction](#transport-abstraction).

It is focused on:

- **Reliability** - The bridge must stream NMEA/RTCM in the field without dropouts
- **Low power** - Aggressive power saving to maximize battery life (target ~20-30h)
- **Compatibility** - Works with mainstream NTRIP/survey apps (Lefebure, SW Maps, etc.)
- **Maintainability** - Readable, well-documented code that's easy to develop

**Hardware**: Wemos D1 Mini32 (ESP32) + Quectel LC29H DA (dual-antenna multi-band RTK) + power management module.

## 🚦 Workflow - ALWAYS START WITH PLANNING

### Step 1: Understand the Task (MANDATORY)

Before making any code changes:

1. **Read the context** - Understand the goal and its impact on the bridge
2. **Analyze dependencies** - Decide whether it touches UART, a transport, the WiFi setup window/OTA, or power
3. **Review existing code** - Don't duplicate functionality, use what exists
4. **Mind power, memory & timing** - Changes must not break the setup window, the heap budget or data throughput

### Step 2: Plan Implementation (MANDATORY)

Create an **action plan** before coding:

```markdown
1. Read module X and understand its operation
2. Add new function to class Y
3. Update configuration in include/config.h
4. Test via serial monitor + BLE app + RTK fix
5. Update README.md
```

**Why is this important?**

- Avoids chaotic changes and "shotgun debugging"
- Easier to roll back if something goes wrong
- Code review is simpler

### Step 3: Implement Incrementally

- **One task at a time** - Don't jump between tasks
- **Commit frequently** - Small, atomic changes
- **Test after each step** - Don't proceed if something doesn't work
- **Update documentation** - As you go, not at the end

### Step 4: Verify

- Check serial monitor logs (115200 baud)
- Connect a BLE app and confirm NMEA flows out / RTCM flows in
- Confirm RTK fix still converges with corrections
- Watch for memory issues (`/api/status` reports `freeHeap`, `maxAlloc`, `minFreeHeap`)

## 🏗️ System Architecture

### Data Flow (Bidirectional Bridge)

```
                     ┌──────────────────────┐
LC29H DA  ──NMEA──►  │        ESP32         │  ──BLE NUS TX──►  Mobile App
(GNSS)              │  UART ↔ BLE bridge    │
          ◄─RTCM──  │                      │  ◄─BLE NUS RX──   (NTRIP corrections)
                     └──────────────────────┘
```

- **UART → BLE (TX char)**: NMEA sentences and status from the GNSS to the phone.
- **BLE (RX char) → UART**: RTCM corrections from the phone's NTRIP client to the GNSS.

### Boot Sequence: WiFi Setup Window, Then Bluetooth (CRITICAL)

WiFi and Bluetooth share **one 2.4 GHz radio** and most of the heap. Running them together was measured on this hardware and does not work: ~34KB free heap, truncated web responses, failing OTA, and seconds of WiFi latency once a BLE client streams. So they never run at the same time:

```
power-on ──► SETUP WINDOW (WiFi only: web UI, OTA, Telnet; ~125KB free heap)
                │  idle for setupWindowSec (default 30s), or "Start Bluetooth now" in the UI
                ▼
             WiFi OFF ──► Bluetooth transport starts (BLE or SPP) ──► bridge runs
```

- The window is an **idle timeout**. It is extended only by real use: the web UI heartbeat (`GET /api/status?ka=1`, sent while the page is visible and was touched in the last 5 minutes), config/scan requests, an OTA transfer, or a Telnet client. **A phone that merely joins the WiFi does not extend it.**
- WiFi does not come back until the next restart.
- **WiFi TCP transport is the exception**: no Bluetooth is started at all, WiFi stays on, the bridge starts at boot.
- During the window the LED gives a short flash once per second and GNSS output is discarded.

**⚠️ Do not start a Bluetooth stack while WiFi is up**, and do not add features that need both at once.

### Main Components

```
include/
  config.h                  → pins, factory defaults, WiFi/OTA, power, BLE constants
data/                       → web UI (plain HTML/CSS/JS, served from LittleFS)
partitions.csv              → 2 × 1.81MB OTA slots + 256KB LittleFS
src/
  main.cpp                  → setup + loop orchestration, setup window → transport hand-over
  settings.cpp/h            → RoverSettings + NVS persistence (SettingsStore)
  wifi_manager.cpp/h        → AP / STA / AP+STA, AP fallback, setup window timeout
  web_server.cpp/h          → static UI + REST API (/api/status, /api/config, ...)
  ota_service.cpp/h         → ArduinoOTA + mDNS
  telnet_service.cpp/h      → remote log console (status / restart / help)
  logger.cpp/h              → logPrint / logPrintln to Serial + Telnet mirror
  system_control.cpp/h      → deferred restart
  bridge.cpp/h              → GNSS UART ↔ transport, link status LED
  uart_handler.cpp/h        → UART comms with LC29H DA
  transport.h               → ITransport interface (no radio stack includes)
  transport_factory.cpp/h   → creates the transport selected in settings
  ble_uart_service.cpp/h    → BLE Nordic UART Service (Bluedroid)
  spp_serial_service.cpp/h  → Bluetooth Classic SPP via BluetoothSerial (Bluedroid)
  tcp_transport.cpp/h       → raw TCP server (NMEA out, RTCM in)
```

### Transport Abstraction

`main.cpp` and `bridge` never name a radio stack. They talk to `ITransport` (in `transport.h`): `begin()`, `setTxPower()`, `sendData()`, `setDataCallback()`, `setLogCallback()`, `isConnected()`, `getConnectedCount()`, `name()`, `loop()`. `createTransport()` picks the implementation from `settings.transport` once per boot; changing it in the web UI saves to NVS and restarts.

| Transport     | Stack            | iOS | Android | Notes                                         |
| ------------- | ---------------- | --- | ------- | --------------------------------------------- |
| BLE (default) | Bluedroid BLE    | ✅  | ✅      | Nordic UART Service, 185B MTU, chunked        |
| SPP           | Bluedroid BR/EDR | ❌  | ✅      | Plain byte stream, pair first                 |
| TCP           | WiFi (lwIP)      | ✅  | ✅      | Port 10110 by default, up to 2 clients, no BT |

Conventions to keep:

- **`transport.h` must stay free of any radio stack include** - every implementation includes it.
- **Radio-specific calls belong inside the implementation.** TX power is a good example: `esp_ble_tx_power_set()` vs `esp_bredr_tx_power_set()` both hide behind `setTxPower()`.
- **Each Bluetooth transport enables the controller for its own mode only.** The Arduino core always starts the dual-mode controller; `begin()` first releases the unused half (`esp_bt_controller_mem_release`) and enables BLE-only / Classic-only itself. That is ~20KB of heap - keep it.
- **Both Bluetooth transports use Bluedroid** so they fit one image. Mixing NimBLE with `BluetoothSerial` in one firmware is discouraged upstream and was not tried here - do not reintroduce it without testing.
- Adding a transport = new `ITransport` implementation + a `TransportMode` value + one branch in `transport_factory.cpp` + the UI option. `main.cpp` should not change.

**⚠️ iOS has no access to Bluetooth Classic SPP without MFi certification.** Never make SPP the default - it would silently drop every iPhone user.

### Runtime Settings and Web API

Everything the UI can change lives in `RoverSettings` (NVS namespace `rover`): transport, TCP port, WiFi mode, STA/AP credentials, setup window length. `config.h` only holds the factory defaults. Passwords are write-only over the API.

| Endpoint              | Purpose                                                        |
| --------------------- | -------------------------------------------------------------- |
| `GET /api/status`     | Live state; `?ka=1` extends the setup window                   |
| `GET /api/config`     | Current settings (no passwords)                                |
| `POST /api/config`    | Validate, save, restart                                        |
| `POST /api/restart`   | Restart without saving                                         |
| `POST /api/wifi/off`  | End the setup window now (WiFi off, Bluetooth starts)          |
| `GET /api/wifi/scan`  | Async network scan, poll until `scanning` is false             |

There is **no authentication** on the web API yet, and the default AP is open.

### WiFi Modes

`AP`, `STA` or `AP+STA` (default). If a configured station cannot join its network within `WIFI_TIMEOUT_MS` (10s) the rover drops the station and runs as a plain AP (`OSSRTK-XXXX`), so the UI stays reachable.

### UART Pinout (ESP32 ↔ LC29H DA)

```
LC29H DA UART1_TX → GPIO25 (ESP32 RX, GNSS data in)
LC29H DA UART1_RX ← GPIO27 (ESP32 TX, corrections out)
Baud 115200, 8N1, no flow control
```

### Nordic UART Service (NUS) UUIDs

```
Service:  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
RX Char:  6E400002-B5A3-F393-E0A9-E50E24DCCA9E  (Mobile → ESP32 → GNSS)
TX Char:  6E400003-B5A3-F393-E0A9-E50E24DCCA9E  (GNSS → ESP32 → Mobile)
```

**⚠️ These UUIDs are the standard NUS. Changing them breaks app compatibility.**

## ⚡ Power Management (CRITICAL)

| Feature                | Behavior                                                             |
| ---------------------- | -------------------------------------------------------------------- |
| WiFi setup window      | WiFi only for `setupWindowSec` after boot (default 30s), then off    |
| Low Bluetooth TX power | -9 dBm (~10-15m range, sufficient rover→phone)                       |

The WiFi TCP transport keeps WiFi on permanently and draws accordingly. Bluetooth Classic (SPP) keeps a costlier link up than BLE. Measure before quoting battery numbers.

```cpp
// Power-related defines live in include/config.h:
#define BLE_TX_POWER ESP_PWR_LVL_N9      // -9dBm low power (BLE)
#define SPP_TX_POWER ESP_PWR_LVL_N9      // -9dBm low power (SPP)
#define DEFAULT_SETUP_WINDOW_SEC 30      // WiFi-only window after boot
```

> Earlier revisions of this document described 80/240 MHz CPU frequency scaling. It is not implemented in the firmware.

## 📝 Coding Standards

### C++ Style

```cpp
// GOOD: CamelCase for classes, camelCase for methods
class BleUartService {
public:
    void begin();
    bool sendToMobile(const uint8_t* data, size_t len);
};

// GOOD: snake_case for local variables
size_t bytes_read = 0;

// GOOD: UPPER_SNAKE_CASE for constants / #defines (in config.h)
#define BLE_CHUNK_SIZE 180

// GOOD: Comments explain "why", not "what"
// iOS caps MTU at 185, so chunk large RTCM frames before notifying
```

### Configuration

**Compile-time tunables and factory defaults live in `include/config.h`** (pins, WiFi/OTA, power, BLE). Don't scatter magic numbers across modules — add a `#define` and reference it. Anything the user should change in the field belongs in `RoverSettings` + the web UI instead.

### Memory Management

ESP32 has limited RAM (~320KB usable). Bluedroid is heavy, which is exactly why WiFi and Bluetooth never run together (see the boot sequence). Measured free heap: ~125KB in the setup window; with both radios up it was ~34-57KB and the web server / OTA failed.

```cpp
// GOOD: F() macro keeps literals in flash
Serial.println(F("[BLE] Nordic UART Service started"));

// GOOD: fixed-size buffers sized to UART_BUF_SIZE / BLE_CHUNK_SIZE
uint8_t buffer[UART_BUF_SIZE];
```

### BLE / Chunking Specific

```cpp
// BLE MTU is 185 (iOS max); chunk size 180. Large RTCM messages
// must be split into BLE_CHUNK_SIZE pieces before notify().

// GOOD: only push to mobile when a client is connected
if (deviceConnected) {
    sendToMobile(data, len);
}
```

### Error Handling

```cpp
// GOOD: check init results and signal via LED + serial
if (!uartInit()) {
    Serial.println(F("[ERROR] UART init failed"));
    // fast-blink LED 100ms (see Status LED patterns)
}
```

## 🚀 Build & Upload

Run from the monorepo root. `oss-rover` uploads over OTA, `oss-rover-usb` over serial - same image.

### First upload (USB)

```bash
pio run -e oss-rover-usb -t upload      # firmware
pio run -e oss-rover-usb -t uploadfs    # web UI (LittleFS)
pio device monitor                      # serial @115200
```

A serial flash is also required whenever `partitions.csv` changes - OTA cannot rewrite the partition table.

### OTA upload (WiFi)

```bash
pio run -e oss-rover -t upload          # targets ossrtk.local, auth=admin
pio run -e oss-rover -t uploadfs
```

OTA only works during the setup window: restart the rover and start the upload within `setupWindowSec`. Compiling can eat the whole window, so either build first (`pio run -e oss-rover`) or open the web UI, which keeps WiFi up, and then upload.

> ⚠️ Run `pio device monitor` manually in a terminal — never as a blocking automation step. Opening the serial port resets the board.

## 🐛 Debugging

### Serial Output Conventions

```cpp
Serial.println(F("[WiFi] Connected!"));        // module-tagged status
Serial.println(F("[BLE] iOS device connected!"));
Serial.println(F("[ERROR] UART init failed")); // error
```

### Status LED Patterns

| Pattern               | Meaning                                 |
| --------------------- | --------------------------------------- |
| Short flash every 1s  | Setup window: WiFi only, no client link |
| Blinking (250ms)      | Waiting for a mobile connection         |
| Solid ON              | Mobile device connected                 |
| Fast blinking (100ms) | Error - UART init failed                |
| Fast blinking (200ms) | Error - transport (BLE/SPP) init failed |

### Testing Checklist

After each change, verify:

- [ ] `pio run -e oss-rover` compiles without warnings and still fits the 1.81MB app slot
- [ ] Setup window: web UI loads, `/api/status` shows `transport.active: false`, free heap is ~120KB
- [ ] Window closes on its own after `setupWindowSec` without use, then the transport starts
- [ ] An open web UI keeps the window open; "Start Bluetooth now" ends it
- [ ] OTA works for firmware and filesystem inside the window
- [ ] Device `OSSRTK` is discoverable and connects (BLE: advertises; SPP: pairs, then port opens)
- [ ] NMEA flows GNSS → phone and RTCM flows phone → GNSS; RTK fix converges
- [ ] Changing the transport / WiFi mode in the UI survives the restart

### Note on RTK accuracy

A poor/no RTK fix is usually **not** a firmware bug. Check sky view, that corrections are actually arriving over BLE, constellation match, base distance (<40km), and allow 30-120s for convergence.

## 📚 Dependencies and Libraries

Board `wemos_d1_mini32`, envs `oss-rover` (OTA) and `oss-rover-usb` (serial) in the monorepo root `platformio.ini`; `firmware/rover/platformio.ini` is the standalone copy - keep the two in sync.

```ini
lib_deps =
    bblanchon/ArduinoJson@6.21.6
    ESP32Async/ESPAsyncWebServer@3.7.10
    ESP32Async/AsyncTCP@3.4.10
    BLE / BluetoothSerial / WiFi / FS / LittleFS / Preferences / ESPmDNS / ArduinoOTA   ; framework-bundled

board_build.partitions = firmware/rover/partitions.csv
board_build.filesystem = littlefs
```

**⚠️ Flash is tight**: the image uses ~92% of the 1.81MB app slot. Check the size line after adding anything.

**⚠️ The web UI must stay small and build-free.** LittleFS is 256KB; the UI is ~55KB of hand-written HTML/CSS/JS styled after the transmitter UI. Do not port the transmitter's React bundle (~800KB).

**⚠️ New envs need a `scripts/set_env_dirs.py` entry**, otherwise `include/config.h` and the `data/` directory are not picked up in a monorepo root build.

**⚠️ Don't update libraries without testing!** ESPAsyncWebServer/AsyncTCP lose data under memory pressure (open upstream issue) - verify UI load and OTA on hardware after any bump.

## 🔐 Security Notes

Factory-default credentials and OTA settings live in `include/config.h`; the WiFi ones can be overridden in the web UI (stored in NVS):

```cpp
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#define OTA_HOSTNAME "ossrtk"      // ossrtk.local
#define OTA_PASSWORD "admin"       // change before deployment
```

- This is an open-source repo — don't commit real WiFi/OTA secrets.
- The OTA password protects firmware uploads; change it from the default for any real device.
- The web UI / REST API has no authentication and the default AP is open. Exposure is limited to the setup window (or permanently in WiFi TCP mode).
- BLE NUS is unauthenticated by design (app compatibility); keep TX power low to limit range.

## 📖 Documentation

**Always** update `README.md` when you change behavior:

| Change               | Update                          |
| -------------------- | ------------------------------- |
| Pins / UART config   | README Pinout + config section  |
| Power behavior       | README Power Optimization       |
| BLE UUIDs / chunking | README NUS section              |
| New feature          | README Overview + Configuration |

### Comment Format

```cpp
/**
 * @brief Forward RTCM bytes received over BLE to the GNSS UART.
 * @param data Pointer to RTCM payload from the mobile app
 * @param length Data length in bytes
 */
```

## ⚠️ Common Pitfalls

### 1. Running WiFi and Bluetooth together

```cpp
// BAD: starting BLE/SPP while WiFi is up, or keeping WiFi on "just for status"
// → one radio, ~34KB heap: truncated HTTP responses, failed OTA, laggy link.
// GOOD: WiFi only in the setup window; the transport starts after WiFi is off.
```

### 1b. Blocking the main loop

`UARTHandler::readData()` waits for the full requested length (1s timeout). Always clamp the length to `available()`. The loop also serves OTA, Telnet and the setup window timeout.

### 2. Ignoring BLE MTU / chunk size

```cpp
// BAD: notify() with a frame larger than the negotiated MTU
// GOOD: split into BLE_CHUNK_SIZE (180B) pieces
```

### 3. Changing NUS UUIDs or device name casually

Mobile apps discover the rover by the standard NUS UUIDs and the `BT_DEVICE_NAME` (`OSSRTK`). Changing them breaks existing app setups.

### 4. Watchdog Timeout

ESP32 has a watchdog — long blocking work resets the device. Keep the bridge loop responsive; yield in long loops.

### 5. `F()` Macro for String Literals

```cpp
// BAD: Serial.println("uses RAM");
// GOOD: Serial.println(F("stored in flash"));
```

### 6. Forgetting the GNSS baud quirk

Default LC29H DA baud is 115200, but some modules ship at 9600 — check the datasheet before assuming no-data is a firmware bug.

## 🤖 AI Agent Specific Guidelines

1. **Always read context** - Use `read_file` on `main.cpp`, the relevant module, `transport.h` and `config.h` before changing behavior.
2. **Plan before action** - List steps before editing.
3. **Small changes** - One feature = several small commits.
4. **Mind power, memory & compatibility** - Confirm the setup window, heap budget and BLE/NUS compatibility aren't broken.
5. **Follow existing patterns** - Don't introduce new styles where one is established.

### If You're Not Sure

**Ask the user** instead of guessing:

- "Should this keep WiFi alive longer? It delays Bluetooth and costs power."
- "Does this change affect BLE app compatibility?"
- "Should I update the README too?"

### Context Gathering

Before a major change, read:

- `src/main.cpp` - boot, setup window → transport hand-over
- `include/config.h` - compile-time parameters and factory defaults
- `src/settings.*`, `src/wifi_manager.*`, `src/web_server.*` - runtime settings, WiFi window, REST API
- `src/transport.h` + `src/transport_factory.*` - the transport contract and selection
- `src/uart_handler.*` for GNSS comms, `src/ble_uart_service.*` / `src/spp_serial_service.*` / `src/tcp_transport.*` for the links
- `data/` for the web UI, `README.md` for the wiring, power model, and app setup

---

**Maintainer**: OSS Rover Team

**Remember**: This is a low-power field device. Every change is also a power decision. 🚀
