# AGENTS.md - OSS Receiver Project Standards

Guidelines for AI agents (GitHub Copilot, Cursor, Claude, etc.) and developers working on this project.

## 🎯 Project Philosophy

**OSS Receiver** is professional embedded firmware for the ESP32, acting as a **LoRa → NTRIP gateway**. It receives RTCM correction data over LoRa radio, reassembles fragmented messages, and serves them to GNSS rovers via a local NTRIP caster over WiFi.

It is focused on:

- **Reliability** - The caster must stream valid RTCM to rovers without interruption in the field
- **Real-time correctness** - LoRa reception is time-critical and must never be starved by network/UI work
- **Modularity** - Clean separation between radio, parsing, network, and UI layers
- **Maintainability** - Readable, well-documented code that's easy to develop

**Hardware**: TTGO LoRa32 V2.1 (ESP32 + SX1276 + SSD1306 OLED)

## 🚦 Workflow - ALWAYS START WITH PLANNING

### Step 1: Understand the Task (MANDATORY)

Before making any code changes:

1. **Read the context** - Understand the goal and its impact on the system
2. **Analyze dependencies** - Check which of the two FreeRTOS tasks/cores are affected
3. **Review existing code** - Don't duplicate functionality, use what exists
4. **Understand the dual-core split** - Changes must not break inter-core synchronization

### Step 2: Plan Implementation (MANDATORY)

Create an **action plan** before coding:

```markdown
1. Read module X and understand its operation
2. Add new function to module Y
3. Update configuration in config.h
4. Test change via serial monitor + NTRIP client
5. Update docs/ documentation
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

- Check serial monitor logs (both cores log through `debugMutex`)
- Connect an NTRIP client and confirm RTCM streaming
- Ensure the OLED display still updates
- Watch for stack overflows / watchdog resets (ESP32 only has 520KB RAM!)

## 🏗️ System Architecture

### Dual-Core Design (CRITICAL)

The firmware runs two FreeRTOS tasks pinned to separate cores. **Every change must respect this split.**

```
Core 1 (loraTask, prio 2)            Core 0 (networkTask, prio 1)
─────────────────────────            ────────────────────────────
checkForPackets()  (SPI)             xQueueReceive(rtcmQueue)
onReceive() → reassembly             broadcastRTCMData() → clients
processRTCMData()                    handleNTRIPClients()
parseNextRTCMMessage()               handleWiFiClients()
buzzerUpdate() (GPIO)                updateDisplay() (I2C/OLED)
checkMessageTimeout()                removeDisconnectedClients()
          │                                     ▲
          └──────────► rtcmQueue ───────────────┘
                       (FreeRTOS Q)
```

- **Core 1 — LoRa Reception (time-critical, priority 2)**: all radio SPI I/O, packet reassembly, RTCM parsing/CRC, enqueue to `rtcmQueue`, buzzer GPIO.
- **Core 0 — Network & UI (priority 1, shares core with WiFi stack)**: dequeue from `rtcmQueue`, broadcast to NTRIP clients, accept connections, NTRIP protocol, OLED refresh.

### Module Overview

| Module       | File(s)              | Responsibility                                            |
| ------------ | -------------------- | --------------------------------------------------------- |
| Main         | `main.cpp`           | FreeRTOS task creation, setup sequence                    |
| LoRa Radio   | `lora_radio.cpp/h`   | SPI radio, packet reassembly, RTCM parsing pipeline       |
| RTCM Parser  | `rtcm_parser.cpp/h`  | Circular buffer, RTCM3 frame parsing, CRC-24Q validation  |
| NTRIP Server | `ntrip_server.cpp/h` | TCP server, NTRIP protocol, client management, broadcast  |
| WiFi Manager | `wifi_manager.cpp/h` | Access Point setup, client tracking                       |
| Display      | `display.cpp/h`      | SSD1306 OLED driver, statistics rendering                 |
| Buzzer       | `buzzer.cpp/h`       | Non-blocking GPIO pulse on valid RTCM frame               |
| Debug        | `debug.cpp/h`        | Thread-safe Serial logging macros                         |
| Config       | `config.h`           | All compile-time constants (pins, radio, network, timing) |

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

**⚠️ Never share state between the two tasks without one of these mechanisms.**

### Boot Sequence (`setup()`)

```
initDebug() → setupOLED() → showSplashScreen() → setupWiFiHotspot()
→ setupNTRIPServer() → initRTCMBuffer() → setupBuzzer() → setupLoRa()
→ xQueueCreate() → xTaskCreatePinnedToCore(loraTask, Core 1)
→ xTaskCreatePinnedToCore(networkTask, Core 0)

loop() → vTaskDelete(NULL)   // Arduino loop unused
```

## 📝 Coding Standards

### C++ Style

```cpp
// GOOD: CamelCase for classes/structs, camelCase for functions
void broadcastRTCMData(const uint8_t* data, size_t len);

// GOOD: snake_case for local variables
uint32_t bytes_received = 0;

// GOOD: UPPER_SNAKE_CASE for constants / #defines (all live in config.h)
#define NTRIP_PORT 2101
#define MAX_MESSAGE_BUFFER 16384

// GOOD: Comments explain "why", not "what"
// SF7 + 500kHz keeps airtime low enough to relay full RTCM stream
```

### Configuration

**All compile-time constants live in `include/config.h`.** Do not scatter magic numbers across modules.

```cpp
// GOOD: reference the define
delay(DISPLAY_UPDATE_INTERVAL);

// BAD: hardcoded literal
delay(500);
```

When adding tunable behavior, add a `#define` to `config.h` and document it in `docs/configuration.md`.

### Memory Management

ESP32 has **only 520KB RAM** - every byte counts, and two 8KB task stacks are already reserved.

```cpp
// GOOD: F() macro keeps string literals in flash
Serial.println(F("[LoRa] packet received"));

// GOOD: stack allocation for transient buffers
uint8_t buffer[MAX_PACKET_SIZE];

// CAUTION: heap allocation must always be freed
// BAD: allocating in the hot LoRa path on every packet
```

### Thread-Safe Logging

Both cores log to the same Serial port. **Always use the debug macros** (which take `debugMutex`) instead of raw `Serial.print`:

```cpp
// GOOD: thread-safe via debug.h macros
DEBUG_PRINTLN(F("[NTRIP] client connected"));

// BAD: raw Serial from a task can interleave with the other core
Serial.println("client connected");
```

### LoRa Radio Specific

```cpp
// GOOD: all LoRa SPI access stays on Core 1 (loraTask) only
// BAD: calling LoRa.* from networkTask (Core 0) — SPI is not shared!

// Reassembly matches fragments by Message ID; respect PACKET_TIMEOUT
// for dropping incomplete multi-packet messages.
```

## 📡 LoRa Protocol

The receiver must stay byte-compatible with the **transmitter's** packet format. The LoRa protocol is a **shared spec** — the canonical source is [`../../shared/docs/lora-protocol.md`](../../shared/docs/lora-protocol.md) (see also `docs/configuration.md`):

| Parameter        | Value                   |
| ---------------- | ----------------------- |
| Frequency        | 433.7 MHz (`433700000`) |
| Spreading Factor | 7                       |
| Bandwidth        | 500 kHz                 |
| Coding Rate      | 4/5                     |
| Sync Word        | 0x12                    |
| Header size      | 4 bytes                 |
| Max payload      | 508 bytes/packet        |

**⚠️ If you change any radio parameter, the transmitter must change to match, or the link breaks silently.**

## 🌐 NTRIP Server

- Caster listens on TCP port `2101`, mount point `LORA`, max 4 clients.
- `GET / HTTP/1.1` → source table, then close.
- `GET /LORA HTTP/1.1` → `ICY 200 OK` + binary RTCM stream (connection stays open).
- No client slot free → `503 Service Unavailable` → close.

See `docs/ntrip-server.md` for the full protocol flow and rover configuration.

## 🐛 Debugging

### Serial Output Conventions

```cpp
DEBUG_PRINTLN(F(">>> LoRa init..."));      // status
DEBUG_PRINTLN(F("[NTRIP] client added"));  // module-tagged event
DEBUG_PRINTLN(F("[ERROR] CRC failed"));    // error
```

Serial baud: 115200. Set `ENABLE_UART_LOGGING false` in `config.h` to silence all output.

### Testing Checklist

After each change, verify:

- [ ] Compiles without warnings (`pio run -e ttgo-lora32-v21`)
- [ ] Both tasks start (serial shows loraTask + networkTask)
- [ ] LoRa frames are received and pass CRC (RX rate on OLED > 0)
- [ ] NTRIP client connects and receives RTCM stream
- [ ] OLED statistics update every 500ms
- [ ] No watchdog resets / stack overflow warnings

## 📚 Dependencies and Libraries

Target environment: `ttgo-lora32-v21`

```ini
lib_deps =
    thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays@^4.4.0
    sandeepmistry/LoRa@^0.8.0
```

**⚠️ Don't update libraries without testing!** ESP32 embedded code often breaks on library updates. If you must update: test on a branch, read release notes, verify LoRa + NTRIP + OLED, and commit with a clear "tested OK" message.

## 🔧 Build & Upload

```bash
# Build
pio run -e ttgo-lora32-v21

# Upload (serial)
pio run -e ttgo-lora32-v21 -t upload

# Monitor
pio device monitor --baud 115200
```

> ⚠️ Do not run the serial monitor as a blocking step inside automation — run it manually in a terminal.

## 🔐 Security Notes

The WiFi AP credentials and NTRIP settings live in `config.h`:

```cpp
#define WIFI_SSID "OSS-LoRa-RX"
#define WIFI_PASSWORD "lora123456"   // change before any real deployment
```

- The NTRIP caster currently requires **no authentication** on the mount point. If exposing beyond a closed field network, add client authentication and rate limiting.
- Don't hardcode secrets you don't want published — this is an open-source repo.

## 📖 Documentation

The `docs/` folder is the source of truth. **Always** update it when you change behavior:

| Change                       | Update                                        |
| ---------------------------- | --------------------------------------------- |
| Radio params / packet format | `../../shared/docs/lora-protocol.md` (shared) |
| Pins, timing, constants      | `docs/configuration.md`                       |
| Task/core structure          | `docs/architecture.md`                        |
| NTRIP behavior               | `docs/ntrip-server.md`                        |
| Maintenance/heartbeat frame  | `../../shared/docs/maintenance-frame.md`      |

### Comment Format

```cpp
/**
 * @brief Reassemble LoRa fragments into a complete message.
 * @param data Pointer to packet payload (after 4-byte header)
 * @param length Payload length in bytes
 * @return true when a full message is reassembled
 */
```

## ⚠️ Common Pitfalls

### 1. Touching LoRa SPI from the wrong core

```cpp
// BAD: LoRa.* called from networkTask (Core 0)
// SPI radio access belongs exclusively to loraTask (Core 1).
```

### 2. Sharing state without synchronization

```cpp
// BAD: networkTask reading stats while loraTask writes them
// GOOD: guard with statsMux (portMUX) or pass via rtcmQueue
```

### 3. Blocking the time-critical LoRa task

```cpp
// BAD: long delay()/network call inside loraTask — drops incoming packets
// Keep Core 1 lean; push slow work to Core 0 via the queue.
```

### 4. Watchdog Timeout

ESP32 has a watchdog — if a task blocks too long, the system resets. Yield (`vTaskDelay`) in long loops.

### 5. Raw `Serial.print` from a task

Interleaves with the other core. Use the `debug.h` macros that take `debugMutex`.

### 6. `F()` Macro for String Literals

```cpp
// BAD: Serial.println("uses RAM");
// GOOD: Serial.println(F("stored in flash"));
```

## 🤖 AI Agent Specific Guidelines

1. **Always read context** - Use `read_file` on `main.cpp`, the relevant module, and `config.h` before changing behavior.
2. **Plan before action** - List steps before editing.
3. **Small changes** - One feature = several small commits.
4. **Respect the dual-core boundary** - Decide which core owns new code before writing it.
5. **Follow existing patterns** - Don't introduce new styles where one is established.

### If You're Not Sure

**Ask the user** instead of guessing:

- "Should this run on the LoRa core or the network core?"
- "Does this radio change need a matching transmitter update?"
- "Should I update the docs/ files too?"

### Context Gathering

Before a major change, read:

- `src/main.cpp` - task creation + boot sequence
- `include/config.h` - all parameters
- The relevant module `.cpp`/`.h`
- `docs/architecture.md` for the core split
- `../../shared/docs/lora-protocol.md` (shared spec) if changes involve the radio link

---

**Maintainer**: OSS Receiver Team

**Remember**: The LoRa core must never starve. Keep radio I/O lean, push everything else through the queue. 🚀
