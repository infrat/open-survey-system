# AGENTS.md - OSS Rover Project Standards

Guidelines for AI agents (GitHub Copilot, Cursor, Claude, etc.) and developers working on this project.

## 🎯 Project Philosophy

**OSS Rover** is professional embedded firmware for the ESP32. It is a bidirectional **UART ↔ Bluetooth bridge** that connects a Quectel **LC29H DA** GNSS RTK receiver to iOS/Android survey apps, over either the **Nordic UART Service (NUS)** on BLE (default) or **Bluetooth Classic SPP** (Android only). The transport is a compile-time choice - see [Transport Abstraction](#transport-abstraction).

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
2. **Analyze dependencies** - Decide whether it touches UART, BLE, WiFi/OTA, or power
3. **Review existing code** - Don't duplicate functionality, use what exists
4. **Mind power & timing** - Changes must not break power scaling or data throughput

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
- Watch for memory issues (ESP32 has only 320KB usable RAM)

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

### Main Components

```
include/
  config.h                  → transport selection, pins, WiFi/OTA, power, BLE constants
src/
  main.cpp                  → setup + loop, power management, OTA, orchestration
  uart_handler.cpp/h        → UART comms with LC29H DA
  transport.h               → ITransport interface (no Bluetooth stack includes)
  rover_transport.h         → compile-time transport selection → RoverTransport
  ble_uart_service.cpp/h    → BLE Nordic UART Service (NUS) via NimBLE
  spp_serial_service.cpp/h  → Bluetooth Classic SPP via BluetoothSerial (Bluedroid)
```

| Module               | Responsibility                                            |
| -------------------- | --------------------------------------------------------- |
| `main.cpp`           | Boot, WiFi/OTA window, CPU freq scaling, bridge loop, LED |
| `uart_handler`       | UART1 read/write to the GNSS (GPIO25 RX / GPIO27 TX)      |
| `transport`          | Radio-agnostic contract used by `main.cpp`                |
| `ble_uart_service`   | NUS service, RX/TX characteristics, chunked notifications |
| `spp_serial_service` | SPP server, `onData` callback, unchunked stream writes    |

### Transport Abstraction

`main.cpp` never names a Bluetooth stack. It talks to `RoverTransport`, a typedef resolved in `rover_transport.h`:

```cpp
#if ROVER_TRANSPORT == TRANSPORT_SPP
#include "spp_serial_service.h"
typedef SPPSerialService RoverTransport;
#elif ROVER_TRANSPORT == TRANSPORT_BLE
#include "ble_uart_service.h"
typedef BLEUARTService RoverTransport;
#endif
```

`ITransport` (in `transport.h`) is the whole contract: `begin()`, `setTxPower()`, `sendData()`, `setDataCallback()`, `setLogCallback()`, `isConnected()`, `getConnectedCount()`, `name()`.

Conventions to keep:

- **`transport.h` must stay free of any Bluetooth include** - both implementations include it.
- **Every implementation file is wrapped in its own `#if ROVER_TRANSPORT == ...` guard**, with `#include "config.h"` first. The unused one compiles to an empty translation unit, so the whole `src/` tree can be built by both envs.
- **Application code includes `rover_transport.h`**, never `ble_uart_service.h` / `spp_serial_service.h` directly.
- **Radio-specific calls belong inside the implementation.** TX power is a good example: `esp_ble_tx_power_set()` vs `esp_bredr_tx_power_set()` both hide behind `setTxPower()`.
- Adding a transport = new `ITransport` implementation + one branch in `rover_transport.h` + one env. `main.cpp` should not change.

| Transport                 | env             | Stack     | iOS | Android | Flash   |
| ------------------------- | --------------- | --------- | --- | ------- | ------- |
| `TRANSPORT_BLE` (default) | `oss-rover`     | NimBLE    | ✅  | ✅      | ~1.08MB |
| `TRANSPORT_SPP`           | `oss-rover-spp` | Bluedroid | ❌  | ✅      | ~1.59MB |

**⚠️ iOS has no access to Bluetooth Classic SPP without MFi certification.** Never switch the default env to SPP - it would silently drop every iPhone user.

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

This firmware is built around aggressive power saving. **Do not weaken these without good reason.**

| Feature                | Behavior                                          |
| ---------------------- | ------------------------------------------------- |
| CPU freq scaling       | 80 MHz idle / 240 MHz active (100ms threshold)    |
| WiFi auto-shutdown     | WiFi on for 3 min after boot (OTA), then disabled |
| Low Bluetooth TX power | -9 dBm (~10-15m range, sufficient rover→phone)    |

Approximate budget for the BLE build: ~250mA with WiFi on → ~105mA idle → ~140-180mA active streaming. **The SPP build draws more** - Bluetooth Classic keeps a much costlier link up than BLE, so the 20-30h battery target does not carry over. Measure before quoting numbers for it.

```cpp
// Power-related defines live in include/config.h:
#define BLE_TX_POWER ESP_PWR_LVL_N9    // -9dBm low power (BLE build)
#define SPP_TX_POWER ESP_PWR_LVL_N9    // -9dBm low power (SPP build)
#define CPU_FREQ_ACTIVE 240            // MHz when streaming
#define WIFI_ACTIVE_TIME_MS 180000     // 3 minutes
```

**⚠️ Don't change CPU scaling thresholds or the WiFi window without measuring power and confirming OTA still works.**

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

**All tunables live in `include/config.h`** (pins, WiFi/OTA, power, BLE). Don't scatter magic numbers across modules — add a `#define` and reference it.

### Memory Management

ESP32 has limited RAM (~320KB usable). NimBLE is used specifically because it's lighter (~40KB vs ~100KB for the stock stack).

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

### First upload (USB)

```bash
pio run -e oss-rover              # build BLE variant (default)
pio run -e oss-rover-spp          # build Bluetooth Classic SPP variant
pio run -e oss-rover -t upload    # flash over USB
pio device monitor                # serial @115200
```

### OTA upload (WiFi)

```bash
# WiFi is only active for 3 minutes after boot!
pio run -e oss-rover -t upload    # targets ossrtk.local, auth=admin
```

**Both envs must keep compiling.** A change to `main.cpp`, `config.h` or `transport.h` affects both variants - build both before calling it done.

OTA workflow: reboot the rover → wait for WiFi connect (serial/LED) → upload within 3 minutes → WiFi auto-disables.

> ⚠️ Run `pio device monitor` manually in a terminal — never as a blocking automation step.

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
| Blinking (250ms)      | Waiting for a mobile connection         |
| Solid ON              | Mobile device connected                 |
| Fast blinking (100ms) | Error - UART init failed                |
| Fast blinking (200ms) | Error - transport (BLE/SPP) init failed |

### Testing Checklist

After each change, verify:

- [ ] Both envs compile without warnings (`pio run -e oss-rover`, `pio run -e oss-rover-spp`)
- [ ] Serial shows UART + transport init OK
- [ ] Device `OSSRTK` is discoverable and connects (BLE: advertises; SPP: pairs, then port opens)
- [ ] NMEA flows GNSS → phone (UART → transport)
- [ ] RTCM flows phone → GNSS (transport → UART) and RTK fix converges
- [ ] WiFi auto-disables after 3 min; CPU drops to 80 MHz when idle
- [ ] OTA still works within the 3-minute window

### Note on RTK accuracy

A poor/no RTK fix is usually **not** a firmware bug. Check sky view, that corrections are actually arriving over BLE, constellation match, base distance (<40km), and allow 30-120s for convergence.

## 📚 Dependencies and Libraries

Board `wemos_d1_mini32`, envs `oss-rover` (BLE) and `oss-rover-spp` (SPP) in the monorepo root `platformio.ini`.

```ini
[env:oss-rover]                        ; BLE - NimBLE
lib_deps  = h2zero/NimBLE-Arduino@^1.4.0
build_flags = -DCORE_DEBUG_LEVEL=3
              -DCONFIG_BT_NIMBLE_MAX_CONNECTIONS=1
              -DROVER_TRANSPORT=TRANSPORT_BLE

[env:oss-rover-spp]                    ; Bluetooth Classic SPP - Bluedroid
lib_deps  = BluetoothSerial            ; framework-bundled, must be declared
build_flags = -DCORE_DEBUG_LEVEL=3
              -DROVER_TRANSPORT=TRANSPORT_SPP

board_build.partitions = min_spiffs.csv
```

Built-in: WiFi, ArduinoOTA, esp_bt.

**⚠️ Never put both stacks in one env.** NimBLE and Bluedroid are mutually exclusive Bluetooth hosts - a dual BLE+SPP build would require porting `ble_uart_service` off NimBLE onto Bluedroid.

**⚠️ New envs need a `scripts/set_env_dirs.py` entry**, otherwise `include/config.h` is not on the include path in a monorepo root build.

**⚠️ Don't update libraries without testing!** NimBLE in particular is sensitive — verify BLE connect, throughput, and power after any bump, then commit with a "tested OK" note.

## 🔐 Security Notes

Credentials and OTA settings live in `include/config.h`:

```cpp
#define WIFI_SSID "YourNetwork"
#define WIFI_PASSWORD "YourPassword"
#define OTA_HOSTNAME "ossrtk"      // ossrtk.local
#define OTA_PASSWORD "admin"       // change before deployment
```

- This is an open-source repo — don't commit real WiFi/OTA secrets.
- The OTA password protects firmware uploads; change it from the default for any real device.
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

### 1. Breaking power scaling

```cpp
// BAD: leaving CPU at 240 MHz or WiFi on indefinitely → drains battery
// Respect CPU_FREQ_IDLE and WIFI_ACTIVE_TIME_MS.
```

### 2. Ignoring BLE MTU / chunk size

```cpp
// BAD: notify() with a frame larger than the negotiated MTU
// GOOD: split into BLE_CHUNK_SIZE (180B) pieces
```

### 3. Changing NUS UUIDs or device name casually

Mobile apps discover the rover by the standard NUS UUIDs and the `BT_DEVICE_NAME` (`OSSRTK`). Changing them breaks existing app setups.

### 3b. Leaking a Bluetooth stack include past the guard

```cpp
// BAD: unguarded include in a shared header - breaks the other env's build
#include <NimBLEDevice.h>

// GOOD: config.h first, then the guard, then the stack header
#include "config.h"
#if ROVER_TRANSPORT == TRANSPORT_BLE
#include <NimBLEDevice.h>
#endif
```

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
4. **Mind power & compatibility** - Confirm power scaling and BLE/NUS compatibility aren't broken.
5. **Follow existing patterns** - Don't introduce new styles where one is established.

### If You're Not Sure

**Ask the user** instead of guessing:

- "Should this keep WiFi alive longer, and what's the power tradeoff?"
- "Does this change affect BLE app compatibility?"
- "Should I update the README too?"

### Context Gathering

Before a major change, read:

- `src/main.cpp` - boot, power, OTA, bridge loop
- `include/config.h` - all parameters, including `ROVER_TRANSPORT`
- `src/transport.h` + `src/rover_transport.h` - the transport contract and selection
- `src/uart_handler.*` for GNSS comms, `src/ble_uart_service.*` / `src/spp_serial_service.*` for the radios
- `README.md` for the wiring, power model, and app setup

---

**Maintainer**: OSS Rover Team

**Remember**: This is a low-power field device. Every change is also a power decision. 🚀
