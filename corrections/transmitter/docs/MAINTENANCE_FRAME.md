# Maintenance Frame — Implementation

## Overview

The Maintenance Frame feature extends the OSS RTK Transmitter with a bidirectional telemetry and remote-control channel over the existing LoRa link.

**How it works:**
1. Every N minutes (configurable), the transmitter broadcasts a **maintenance frame** containing basic telemetry.
2. Immediately after, it enters **listen mode** for M seconds (configurable).
3. During the listen window, a receiver on the other side may send a **command frame** back.
4. The transmitter parses the command and dispatches it to a registered handler.

**Deferred stop mode** (optional): after a configurable active TX window (default 5 min) **measured from power-on**, the transmitter halts RTCM transmission and enters `STATE_STANDBY`. In standby the radio stays in continuous RX and periodic maintenance frames are still sent. Transmission resumes on receipt of `CMD_START`.

> **Important:** `CMD_START` permanently cancels the deferred-stop timer for the current session (`deferredStopCancelled = true`). After a manual start the device transmits indefinitely — the active TX window no longer applies until the next power cycle. Only `CMD_STOP` or a power cycle can return the device to standby.

---

## 1. Protocol Changes

### Previous packet header (4 bytes):
```
[msgID 1B][totalPkts 1B][pktNum 1B][dataLen 1B][data 0–246B]
```

### New packet header (5 bytes — Protocol v2):
```
[frameType 1B][msgID 1B][totalPkts 1B][pktNum 1B][dataLen 1B][data 0–245B]
```

| `frameType` | Symbol | Description |
|-------------|--------|-------------|
| `0x01` | `FRAME_RTCM` | RTCM correction data (existing traffic) |
| `0x02` | `FRAME_MAINTENANCE` | Telemetry broadcast (TX → air) |
| `0x03` | `FRAME_COMMAND` | Control command (RX → transmitter) |
| `0x04` | `FRAME_ACK` | Optional acknowledgement |

`HEADER_SIZE` changed from 4 → 5; `MAX_DATA_PER_PACKET` from 246 → 245.

---

## 2. Payload Formats

### 2a. Maintenance Frame Payload (18 bytes fixed + TLV extension)

```
+----------+----------+-------+------+----------+----------+-------+-----------+
| uptime   | batt_mv  | rssi  | snr  | rtcm_rx  | rtcm_tx  | flags | reserved  |
| uint32   | uint16   | int8  | int8 | uint32   | uint32   | uint8 | uint8[1]  |
+----------+----------+-------+------+----------+----------+-------+-----------+
  4B         2B         1B      1B     4B          4B         1B      1B   = 18B
```

**`flags` bitmask:**
- bit 0 — `ntrip_connected`
- bit 1 — `wifi_connected`
- bit 2 — `battery_valid`
- bits 3–7 — reserved

**TLV extension block** (variable length, follows fixed part):
```
[tag 1B][len 1B][value lenB] ...
```

| Tag | Content | Built-in |
|-----|---------|----------|
| `0x01` | Custom label string (max 32 chars) | no |
| `0x02` | Free heap (uint32 LE) | **yes** |
| `0x03` | Chip temperature (int8, °C) | no |
| `0x04` | RTCM frames overwritten count (uint32 LE) | no |

Custom TLV providers can be added at runtime via `registerTLVField()`.

### 2b. Command Frame Payload

```
[frameType=0x03 1B][msgId 1B][totalPkts 1B][pktNum 1B][dataLen 1B][commandId 1B][payloadLen 1B][payload 0–243B]
```

| ID | Symbol | Payload | Default enabled |
|----|--------|---------|----------------|
| `0x01` | `CMD_RESET` | none | ✓ |
| `0x02` | `CMD_SLEEP` | `uint32` LE seconds | ✗ |
| `0x03` | `CMD_HTTP_GET` | null-terminated URL | ✗ |
| `0x04` | `CMD_SET_PARAM` | `key\0value\0` | ✗ |
| `0x05` | `CMD_PING` | none | ✓ |
| `0x06` | `CMD_LOGLEVEL` | `uint8` level | ✗ |
| `0x07` | `CMD_START` | none | ✗ |
| `0x08` | `CMD_STOP` | none | ✗ |

Commands not on the `enabledCommands` whitelist are silently ignored.

---

## 3. System States

```cpp
enum SystemState {
    STATE_INIT,
    STATE_RUNNING,          // Normal RTCM TX operation
    STATE_MAINTENANCE_TX,   // Transmitting maintenance frame (Core 0)
    STATE_MAINTENANCE_RX,   // Listening for command in RX mode (Core 0)
    STATE_STANDBY,          // RTCM TX halted, LoRa in continuous RX
    STATE_ERROR
};
```

OLED display strings: `RUNNING` / `MAINT TX` / `MAINT RX` / `STANDBY` / `ERROR`.

---

## 4. Component: `MaintenanceManager`

### Files
- `include/maintenance_manager.h`
- `src/maintenance_manager.cpp`

### Public API

```cpp
class MaintenanceManager {
public:
    MaintenanceManager(ConfigManager& configMgr, volatile SystemState* statePtr);

    // Called from loraTask (Core 0) every iteration when NOT in standby
    void tick();

    // Called from loraTask (Core 0) every iteration when in STATE_STANDBY
    void tickStandby();

    // Register a handler for a command ID (call from setup() before task start)
    void registerCommand(MaintenanceCommand cmd, CommandHandler handler);

    // Push telemetry snapshot from loop() / Core 1 (thread-safe via mutex)
    void updateTelemetry(const MaintenanceTelemetry& telemetry);

    // Register a custom TLV field provider (call from setup())
    void registerTLVField(uint8_t tag, std::function<void(std::vector<uint8_t>&)> provider);

    bool isEnabled() const;
};
```

### `tick()` flow (Core 0, STATE_RUNNING)

```
tick()
  ├── [STATE_STANDBY] → return immediately (tickStandby() owns that state)
  ├── [deferredStop enabled && active window expired]
  │       → currentState = STATE_STANDBY, return
  ├── [maintenance.enabled == false] → return
  ├── [interval not elapsed] → return
  └── [interval elapsed]
        ├─1► currentState = STATE_MAINTENANCE_TX
        │     sendMaintenanceFrame()
        ├─2► currentState = STATE_MAINTENANCE_RX
        │     LoRa.receive()
        │     loop listenDurationMs: parsePacket() → dispatchCommand()
        │     LoRa.idle()
        └─3► currentState = STATE_RUNNING
```

### `tickStandby()` flow (Core 0, STATE_STANDBY)

```
tickStandby()
  ├── [first call after entering standby]
  │       LoRa.receive()  ← enable continuous RX
  ├── [maintenance.enabled && interval elapsed]
  │       LoRa.idle()
  │       currentState = STATE_MAINTENANCE_TX
  │       sendMaintenanceFrame()
  │       currentState = STATE_STANDBY  ← back to standby, RX re-enabled next call
  └── parsePacket() (non-blocking)
        └── if FRAME_COMMAND → dispatchCommand()
              └── CMD_START handler sets currentState = STATE_RUNNING
                    → tickStandby() cleans up RX mode
```

---

## 5. Configuration Structure

```cpp
struct MaintenanceConfig {
    bool     enabled            = false;
    uint32_t intervalMs         = 120000;    // 2 min between maintenance frames
    uint32_t listenDurationMs   = 2000;      // 2 s RX window after each frame
    bool     batteryEnabled     = false;     // ADC battery voltage reading
    uint8_t  batteryPin         = 35;        // ADC1_CH7 on T-Beam v1.1
    float    batteryScaleFactor = 2.0f;      // Voltage divider ratio
    std::vector<uint8_t> enabledCommands;    // Whitelisted CMD_* IDs
    bool     deferredStopEnabled = false;    // Enter standby after active window
    uint32_t activeWindowMs      = 300000;   // Active TX window (default 5 min)
};
```

Serialized under the `maintenance` key in `config.json` (LittleFS).

> **Note:** All JSON documents in `config_manager.cpp` and `web_interface_manager.cpp` use `DynamicJsonDocument(3072)` (heap allocation) to avoid stack overflow on ESP32.

---

## 6. `main.cpp` Integration

### Registered command handlers (setup)

| Command | Action |
|---------|--------|
| `CMD_RESET` | `ESP.restart()` after 500 ms |
| `CMD_PING` | Serial log only |
| `CMD_SLEEP` | `esp_deep_sleep_start()` for N seconds |
| `CMD_LOGLEVEL` | Serial log only (extend as needed) |
| `CMD_START` | `currentState = STATE_RUNNING`; **permanently cancels** deferred-stop timer (`cancelDeferredStop()`) — active window disabled for session |
| `CMD_STOP` | `currentState = STATE_STANDBY` |

### `loraTask` (Core 0)

```cpp
while (true) {
    maintenanceManager->tick();        // handles deferred stop + periodic frame

    if (currentState == STATE_STANDBY) {
        maintenanceManager->tickStandby(); // continuous RX + command dispatch
        vTaskDelay(pdMS_TO_TICKS(1));
        continue;                          // skip RTCM buffer processing
    }
    // ... normal RTCM TX ...
}
```

### `loop()` (Core 1)

- When `STATE_STANDBY`: updates OLED only, `delay(100)`, early return — no WiFi/NTRIP check, no `keepAlive()`.
- `keepAlive()` (GGA to NTRIP) is guarded: only runs when `currentState == STATE_RUNNING`.
- Telemetry pushed to `MaintenanceManager` every `DISPLAY_UPDATE_INTERVAL` ms.

---

## 7. WebUI Changes

### New component: `webui/src/app/components/config/MaintenanceConfig.tsx`

```
Maintenance Frame
├── [toggle]   Enable maintenance frames
├── [select]   Interval:        1 / 2 / 3 / 5 / 10 min
├── [select]   Listen duration: 1 / 2 / 3 / 5 / 10 s
├── [toggle]   Deferred stop
│   └── [select]   Active TX window: 1 / 2 / 5 / 10 / 15 / 30 min
├── [toggle]   Battery monitoring
│   ├── [input]    ADC Pin
│   └── [input]    Scale factor
└── [checkboxes] Allowed commands:
    ☑ RESET    ☑ PING
    ☐ SLEEP    ☐ HTTP GET    ☐ SET PARAM    ☐ LOG LEVEL
    ☐ START    ☐ STOP
```

Tab is accessible via the wrench icon (🔧) in the tab bar (6th column).

---

## 8. Files Changed

| File | Change |
|------|--------|
| `include/maintenance_manager.h` | **NEW** |
| `src/maintenance_manager.cpp` | **NEW** |
| `include/system_state.h` | Added `STATE_MAINTENANCE_TX`, `STATE_MAINTENANCE_RX`, `STATE_STANDBY` |
| `include/config_manager.h` | Added `MaintenanceConfig` struct + field in `SystemConfig` |
| `src/config_manager.cpp` | Defaults/load/save for maintenance config; `DynamicJsonDocument(3072)` |
| `include/lora_transmitter.h` | `LoRaFrameType` enum; `frameType` in `PacketHeader`; `HEADER_SIZE` 4→5 |
| `src/lora_transmitter.cpp` | Updated packet byte layout |
| `src/display_manager.cpp` | `MAINT TX` / `MAINT RX` / `STANDBY` state strings |
| `src/main.cpp` | `MaintenanceManager` instance; handler registration; `tick()`/`tickStandby()` in `loraTask`; telemetry push; standby guard in `loop()`; GGA guard |
| `src/web_interface_manager.cpp` | Maintenance block in GET/POST config; `DynamicJsonDocument(3072)` |
| `webui/src/app/types/config.ts` | `maintenance` field in `Config` interface |
| `webui/src/app/hooks/useConfig.ts` | Default values for maintenance config |
| `webui/src/app/components/config/MaintenanceConfig.tsx` | **NEW** |
| `webui/src/app/App.tsx` | Maintenance tab (6-column grid, wrench icon) |
| `webui/src/app/data/help-content.ts` | Help text for maintenance tab |

---

## 9. Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| **Breaking protocol change** | `frameType` byte added; receivers not updated will reject packets (fire-and-forget, no ACK) |
| **RX window pauses RTCM for 2 s** | Acceptable — rover buffers corrections; 2 s gap does not break RTK fix |
| **Thread safety on telemetry** | `SemaphoreHandle_t` in `MaintenanceManager` guards Core 1 write vs Core 0 read |
| **Unauthenticated commands** | `enabledCommands` whitelist; security-sensitive commands (SLEEP, HTTP GET) off by default |
| **Stack overflow from large JSON** | All JSON docs use `DynamicJsonDocument` (heap); `StaticJsonDocument<4096>` removed |
| **GGA sent during standby** | `keepAlive()` guarded by `currentState == STATE_RUNNING` |


---

## 1. Protocol Changes

### Current packet header (4 bytes):
```
[msgID 1B][totalPkts 1B][pktNum 1B][dataLen 1B][data 0–246B]
```

### New packet header (5 bytes — Protocol v2):
```
[frameType 1B][msgID 1B][totalPkts 1B][pktNum 1B][dataLen 1B][data 0–245B]
```

| `frameType` | Symbol | Description |
|-------------|--------|-------------|
| `0x01` | `FRAME_RTCM` | RTCM correction data (existing traffic) |
| `0x02` | `FRAME_MAINTENANCE` | Telemetry broadcast (TX → air) |
| `0x03` | `FRAME_COMMAND` | Control command (RX → transmitter) |
| `0x04` | `FRAME_ACK` | Optional acknowledgement |

> **Backward compatibility:** Use the `PROTOCOL_V2` build flag in `platformio.ini` to enable the new header. Old receivers can be updated independently.

---

## 2. Payload Formats

### 2a. Maintenance Frame Payload (18 bytes fixed + TLV extension)

```
+----------+----------+-------+------+----------+----------+-------+-----------+
| uptime   | batt_mv  | rssi  | snr  | rtcm_rx  | rtcm_tx  | flags | reserved  |
| uint32   | uint16   | int8  | int8 | uint32   | uint32   | uint8 | uint8[1]  |
+----------+----------+-------+------+----------+----------+-------+-----------+
  4B         2B         1B      1B     4B          4B         1B      1B   = 18B
```

**`flags` bitmask:**
- bit 0 — `ntrip_connected`
- bit 1 — `wifi_connected`
- bit 2 — `battery_valid`
- bits 3–7 — reserved

**TLV extension block** (variable length, follows fixed part):
```
[tag 1B][len 1B][value lenB] ...
```

| Tag | Content |
|-----|---------|
| `0x01` | Custom label string (max 32 chars) |
| `0x02` | Free heap (uint32) |
| `0x03` | Chip temperature (int8, °C) |
| `0x04` | RTCM frames overwritten count (uint32) |

### 2b. Command Frame Payload

```
[commandId 1B][payloadLen 1B][payload 0–243B]
```

| ID | Symbol | Payload |
|----|--------|---------|
| `0x01` | `CMD_RESET` | none |
| `0x02` | `CMD_SLEEP` | `uint32` duration in seconds |
| `0x03` | `CMD_HTTP_GET` | null-terminated URL string |
| `0x04` | `CMD_SET_PARAM` | `key\0value\0` |
| `0x05` | `CMD_PING` | none (response: ACK frame) |
| `0x06` | `CMD_LOGLEVEL` | `uint8` level (0=off, 1=error, 2=debug) |

---

## 3. New Component: `MaintenanceManager`

### Files to create:
- `include/maintenance_manager.h`
- `src/maintenance_manager.cpp`

### Key responsibilities:
- Track elapsed time and trigger the maintenance cycle when due.
- Build and transmit the maintenance frame payload via `LoRaTransmitter`.
- Switch LoRa module to RX mode for the listen window.
- Parse incoming command frames and dispatch to registered handlers.
- Thread-safe telemetry update (Core 1 writes, Core 0 reads via mutex).

### Public API:
```cpp
class MaintenanceManager {
public:
    explicit MaintenanceManager(ConfigManager& configMgr);

    // Called from loraTask (Core 0) — checks timing and executes if due
    void tick();

    // Register a handler for a specific command ID — extensible pattern
    void registerCommand(MaintenanceCommand cmd, CommandHandler handler);

    // Called by main.cpp (Core 1) to push fresh telemetry (thread-safe)
    void updateTelemetry(const MaintenanceTelemetry& telemetry);

    // Register a custom TLV field provider — extensible pattern
    void registerTLVField(uint8_t tag, std::function<void(std::vector<uint8_t>&)> provider);

    bool isEnabled() const;
};
```

### Types:
```cpp
enum class MaintenanceCommand : uint8_t {
    CMD_RESET     = 0x01,
    CMD_SLEEP     = 0x02,
    CMD_HTTP_GET  = 0x03,
    CMD_SET_PARAM = 0x04,
    CMD_PING      = 0x05,
    CMD_LOGLEVEL  = 0x06,
};

struct MaintenanceTelemetry {
    uint32_t uptime_s;
    uint16_t battery_mv;
    uint32_t rtcm_rx_total;
    uint32_t rtcm_tx_total;
    bool     ntrip_connected;
    bool     wifi_connected;
    int8_t   last_rssi;
    int8_t   last_snr;
    uint32_t free_heap;
};

using CommandHandler = std::function<void(const uint8_t* payload, uint8_t len)>;
```

### Internal flow of `tick()`:
```
tick() called from loraTask (Core 0)
  │
  ├── [interval not elapsed] → return immediately
  │
  └── [interval elapsed]
        ├─1─► currentState = STATE_MAINTENANCE_TX
        │     build payload (fixed header + TLV providers)
        │     transmit FRAME_MAINTENANCE via LoRa
        │
        ├─2─► currentState = STATE_MAINTENANCE_RX
        │     LoRa.receive()
        │     loop for listenDurationMs:
        │       parsePacket()
        │       if frameType == FRAME_COMMAND → dispatchCommand()
        │     LoRa.idle()
        │
        └─3─► currentState = STATE_RUNNING
```

---

## 4. Configuration Structure Extension

Add to `config_manager.h` inside `SystemConfig`:

```cpp
struct MaintenanceConfig {
    bool     enabled            = false;
    uint32_t intervalMs         = 120000;   // 2 minutes
    uint32_t listenDurationMs   = 2000;     // 2 seconds
    bool     batteryEnabled     = false;
    uint8_t  batteryPin         = 35;       // ADC1_CH7 on T-Beam v1.1
    float    batteryScaleFactor = 2.0f;     // voltage divider ratio
    std::vector<uint8_t> enabledCommands;   // whitelisted CMD_* IDs
};

// Add to SystemConfig:
MaintenanceConfig maintenance;
```

Update `config_manager.cpp` to serialize/deserialize this block under the `maintenance` JSON key.

---

## 5. Changes to `lora_transmitter.h` / `.cpp`

1. **Add `frameType` field to `PacketHeader`:**
```cpp
struct PacketHeader {
    uint8_t frameType;     // NEW: 0x01=RTCM, 0x02=MAINT, 0x03=CMD
    uint8_t messageId;
    uint8_t totalPackets;
    uint8_t packetNumber;
    uint8_t dataLength;
};
```
`HEADER_SIZE` changes from 4 → 5; `MAX_DATA_PER_PACKET` changes from 246 → 245.

2. **New public method** for raw frame transmission used by `MaintenanceManager`:
```cpp
bool transmitRawFrame(uint8_t frameType, const uint8_t* data, size_t length);
```

---

## 6. Changes to `system_state.h`

```cpp
enum SystemState {
    STATE_INIT,
    STATE_RUNNING,
    STATE_MAINTENANCE_TX,   // Transmitting maintenance frame
    STATE_MAINTENANCE_RX,   // Listening for command (RX mode)
    STATE_ERROR
};
```

Used by `DisplayManager` to show "MAINT TX" / "MAINT RX" on the OLED.

---

## 7. Changes to `main.cpp`

### New global object:
```cpp
MaintenanceManager* maintenanceManager = nullptr;
```

### In `setup()` — instantiate and register command handlers:
```cpp
maintenanceManager = new MaintenanceManager(configManager);

maintenanceManager->registerCommand(MaintenanceCommand::CMD_RESET,
    [](const uint8_t*, uint8_t) {
        Serial.println(F("[MAINT] Remote reset"));
        delay(500);
        ESP.restart();
    });

maintenanceManager->registerCommand(MaintenanceCommand::CMD_PING,
    [](const uint8_t*, uint8_t) {
        Serial.println(F("[MAINT] Ping received"));
    });

maintenanceManager->registerCommand(MaintenanceCommand::CMD_SLEEP,
    [](const uint8_t* p, uint8_t len) {
        if (len >= 4) {
            uint32_t secs;
            memcpy(&secs, p, 4);
            esp_sleep_enable_timer_wakeup((uint64_t)secs * 1000000ULL);
            esp_deep_sleep_start();
        }
    });
```

### In `loraTask()` (Core 0) — add tick at the top of the loop:
```cpp
while (true) {
    maintenanceManager->tick();   // NEW
    // ... existing RTCM buffer handling ...
}
```

### In `loop()` (Core 1) — push telemetry snapshot every second:
```cpp
MaintenanceTelemetry tel;
tel.uptime_s        = millis() / 1000;
tel.rtcm_rx_total   = totalRTCMFramesReceived;
tel.rtcm_tx_total   = totalRTCMFramesTransmitted;
tel.ntrip_connected = ntripClient->isConnected();
tel.wifi_connected  = wifiManager->isConnected();
tel.free_heap       = esp_get_free_heap_size();
maintenanceManager->updateTelemetry(tel);
```

---

## 8. WebUI Changes

### New component: `webui/src/app/components/config/MaintenanceConfig.tsx`

Configuration UI layout:
```
Maintenance Frame
├── [toggle]   Enable maintenance frames
├── [select]   Interval:        1 min / 2 min / 3 min / 5 min / 10 min
├── [select]   Listen duration: 1s / 2s / 3s / 5s / 10s
├── [toggle]   Battery monitoring
│   ├── [input]  ADC Pin (shown when enabled)
│   └── [input]  Scale factor
└── [checkboxes] Enabled commands:
    ☑ RESET    ☑ SLEEP    ☑ PING
    ☐ HTTP GET  ☐ SET PARAM  ☐ LOG LEVEL
```

### Also modify:
- `webui/src/app/App.tsx` — add Maintenance tab
- `webui/src/types/config.ts` — add `MaintenanceConfig` interface
- `webui/src/hooks/useConfig.ts` — extend config load/save

Config data is sent to `POST /api/config` as a `maintenance` JSON block, handled by the existing `handleSaveConfig`.

---

## 9. Files Affected

| File | Change type |
|------|-------------|
| `include/maintenance_manager.h` | **NEW** |
| `src/maintenance_manager.cpp` | **NEW** |
| `include/system_state.h` | Add 2 states |
| `include/config_manager.h` | Add `MaintenanceConfig` to `SystemConfig` |
| `src/config_manager.cpp` | Serialize/deserialize new config block |
| `include/lora_transmitter.h` | Add `frameType` to header, add `transmitRawFrame()` |
| `src/lora_transmitter.cpp` | Implement extended header |
| `src/main.cpp` | Instantiate MM, register handlers, call in `loraTask`, push telemetry |
| `LORA_PROTOCOL.md` | Document new packet format |
| `webui/src/app/components/config/MaintenanceConfig.tsx` | **NEW** |
| `webui/src/app/App.tsx` | Add Maintenance tab |
| `webui/src/types/config.ts` | Extend config types |
| `webui/src/hooks/useConfig.ts` | Extend config load/save |

---

## 10. Risks and Mitigations

| Risk | Mitigation |
|------|-----------|
| **Breaking protocol change** | Use `PROTOCOL_V2` build flag; old receivers can be updated independently |
| **2s RX window pauses RTCM TX** | Acceptable — rover buffers corrections; 2s without new MSM does not break RTK fix |
| **Thread safety on telemetry struct** | `SemaphoreHandle_t` in `MaintenanceManager` guards the struct between Core 1 write and Core 0 read |
| **Unauthenticated commands** | `enabledCommands` whitelist in config; optionally add a 4-byte shared secret field to the CMD frame header |
| **Memory footprint** | `std::map` + `std::function` cost ~500–800 B heap — acceptable within 520 KB RAM |
