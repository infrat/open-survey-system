# AGENTS.md - OSS Transmitter Project Standards

Guidelines for AI agents (GitHub Copilot, Cursor, Claude, etc.) and developers working on this project.

## 🎯 Project Philosophy

**OSS Transmitter** is professional embedded firmware for ESP32, focused on:

- **Reliability** - System must operate reliably in field conditions for extended periods
- **Efficiency** - Optimize battery life and radio bandwidth usage
- **Modularity** - Clean separation of concerns between components
- **Maintainability** - Readable, well-documented code that's easy to develop

## 🚦 Workflow - ALWAYS START WITH PLANNING

### Step 1: Understand the Task (MANDATORY)

Before making any code changes:

1. **Read the context** - Understand the goal and its impact on the system
2. **Analyze dependencies** - Check which components will be affected
3. **Review existing code** - Don't duplicate functionality, use what exists
4. **Understand the state machine** - Changes must not break state logic

### Step 2: Plan Implementation (MANDATORY)

Create an **action plan** using `manage_todo_list`:

```markdown
1. Read module X and understand its operation
2. Add new function to class Y
3. Update configuration file Z
4. Test change via serial monitor
5. Update documentation
```

**Why is this important?**

- Avoids chaotic changes and "shotgun debugging"
- Agent/developer can see progress and adjust approach
- Easier to roll back if something goes wrong
- Code review is simpler

### Step 3: Implement Incrementally

- **One task at a time** - Don't jump between tasks
- **Commit frequently** - Small, atomic changes
- **Test after each step** - Don't proceed if something doesn't work
- **Update documentation** - As you go, not at the end

### Step 4: Verify

- Check serial monitor logs
- Test via web interface (if applicable)
- Ensure state machine works correctly
- Check for memory leaks (ESP32 only has 520KB RAM!)

## 🏗️ System Architecture

### Main Components

```
main.cpp (State Machine Orchestrator)
    ├── ConfigManager      → LittleFS persistence
    ├── WiFiManager        → Network connectivity
    ├── NTRIPClient        → RTCM data source
    ├── RTCMParser         → Message validation
    ├── RTCMBuffer         → Type-based buffering
    ├── LoRaTransmitter    → Radio transmission
    ├── DisplayManager     → OLED UI
    └── WebInterfaceManager → HTTP API + captive portal
```

### State Machine (src/main.cpp)

**Critical**: Every change must respect state flow!

```
STATE_INIT
  ↓ WiFi + NTRIP + LoRa init
STATE_WAITING_FOR_FIRST_RTCM
  ↓ First RTCM message received
STATE_TRANSMISSION (3.5s)
  ↕ Cycle repeats
STATE_GATHERING (0.25s)
```

- **STATE_TRANSMISSION**: RTCM buffer is emptied, all messages transmitted via LoRa
- **STATE_GATHERING**: Radio silence, new RTCM only buffered (no TX)

### Timing Constraints

```cpp
TRANSMISSION_PERIOD_MS = 3500   // TX burst window
GATHERING_PERIOD_MS = 250       // Silence period
GGA_SEND_INTERVAL_MS = 2000     // NTRIP position update
DISPLAY_UPDATE_INTERVAL = 500   // OLED refresh
```

**⚠️ Don't change these values without deep analysis** - they affect rover synchronization!

## 📝 Coding Standards

### C++ Style

```cpp
// GOOD: CamelCase for classes
class NTRIPClient {
public:
    // GOOD: camelCase for methods
    bool connectToCaster();

    // GOOD: snake_case for local variables
    uint32_t bytes_received = 0;

private:
    // GOOD: camelCase for private fields
    String username;
    WiFiClient client;
};

// GOOD: UPPER_SNAKE_CASE for constants
#define MAX_BUFFER_SIZE 1024
const uint32_t DEFAULT_TIMEOUT = 10000;

// GOOD: Comments explain "why", not "what"
// We must delay here to prevent LoRa module overheating
delay(100);

// BAD: Obvious comments
// Set variable to 5
int count = 5;
```

### File Structure

```cpp
// ===== header_file.h =====
#ifndef HEADER_FILE_H
#define HEADER_FILE_H

// Forward declarations (if needed)
class ConfigManager;

// Class definition
class MyComponent {
public:
    // Public API
    MyComponent();
    void initialize();

private:
    // Private implementation
    void internalMethod();

    // Private fields
    int counter;
};

#endif // HEADER_FILE_H

// ===== cpp_file.cpp =====
#include "cpp_file.h"
#include "other_dependencies.h"

// Constructor
MyComponent::MyComponent() : counter(0) {
}

// Public methods
void MyComponent::initialize() {
    // Implementation
}

// Private methods
void MyComponent::internalMethod() {
    // Implementation
}
```

### Memory Management

ESP32 has **only 520KB RAM** - every byte counts!

```cpp
// GOOD: Stack allocation for small buffers
uint8_t buffer[256];

// GOOD: Static for constant strings
static const char* TAG = "MyModule";

// CAUTION: Heap allocation - ensure delete/free!
uint8_t* bigBuffer = new uint8_t[4096];
// ... use buffer ...
delete[] bigBuffer;

// GOOD: String only when really needed
String message = createMessage(); // OK - return and use immediately

// BAD: Unnecessary String copying
String temp1 = getString();
String temp2 = temp1; // unnecessary copy
String temp3 = temp2 + "!"; // another copy
```

### Error Handling

```cpp
// GOOD: Check return values
if (!WiFi.begin(ssid, password)) {
    Serial.println(F("WiFi begin failed"));
    return false;
}

// GOOD: Serial.println for user-facing errors
if (connection_failed) {
    Serial.println(F("[ERROR] Connection timeout"));
}

// GOOD: #ifdef DEBUG for debug logs
#ifdef DEBUG
    Serial.printf("Buffer size: %d bytes\n", bufferSize);
#endif

// BAD: Ignoring errors
ntripClient.connect(); // What if it fails?
```

### LoRa Radio Specific

```cpp
// GOOD: Always check LoRa is not busy
if (LoRa.beginPacket()) {
    LoRa.write(data, length);
    LoRa.endPacket();
}

// GOOD: Delay between packets (SX1276 needs time)
delay(100); // Prevent module overheating

// BAD: Transmission during STATE_GATHERING
if (currentState == STATE_GATHERING) {
    LoRa.beginPacket(); // ERROR! Breaks timing sync!
}
```

## 🧩 Components - Best Practices

### ConfigManager

```cpp
// Access configuration
SystemConfig config = configManager.getConfig();
Serial.println(config.wifi.ssid);

// Save configuration
SystemConfig newConfig = configManager.getConfig();
newConfig.wifi.ssid = "NewNetwork";
configManager.saveConfig(newConfig);

// IMPORTANT: saveConfig() writes to LittleFS - don't call in loops!
```

### RTCMParser + RTCMBuffer

```cpp
// Parser automatically buffers messages by type
rtcmParser->processData(buffer, bytesRead);

// Transmit buffered (only in STATE_TRANSMISSION!)
rtcmParser->transmitBufferedMessagesDirect([](const uint8_t* data, size_t len, uint16_t type) {
    return loraTransmitter->transmitMessage(data, len);
});
```

### LoRaTransmitter

```cpp
// Automatic fragmentation for large messages
bool success = loraTransmitter->transmitMessage(rtcmData, rtcmLength);

// Transmitter handles:
// - Message ID (wrapping 0-255)
// - Fragmentation (packets of 246 bytes)
// - Packet numbering
```

### WebInterfaceManager

```cpp
// REST API endpoints are automatically available:
// GET  /api/config        - Get configuration
// POST /api/config        - Save configuration
// GET  /api/stats         - System statistics
// POST /api/restart       - Restart device

// Adding a custom endpoint:
webInterface->addHandler("/api/custom", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "application/json", "{\"status\":\"ok\"}");
});
```

## 🐛 Debugging

### Serial Output Conventions

```cpp
// User-facing status
Serial.println(F(">>> Connecting to WiFi..."));

// Success
Serial.println(F("✓ WiFi connected"));

// Error
Serial.println(F("✗ NTRIP connection failed"));

// Debug (only if DEBUG defined)
#ifdef DEBUG
    Serial.printf("[DEBUG] Buffer: %d/%d bytes\n", used, total);
#endif

// Statistics
Serial.println(F("=== STATS ==="));
Serial.printf("RX: %lu frames\n", totalFrames);
```

### Common Debug Flags

```cpp
// config.h
#define DEBUG                      // General debug output
#define DEBUG_LORA_PACKETS true    // LoRa packet details
#define DEBUG_LORA_VERBOSE true    // Ultra-verbose (every byte)
```

### Testing Checklist

After each change, verify:

- [ ] Compiles without warnings
- [ ] Serial monitor shows correct logs
- [ ] State machine transitions correctly through states
- [ ] OLED display shows current data
- [ ] Web interface (if touched) works
- [ ] Memory usage hasn't increased drastically
- [ ] LoRa transmits packets (if radio path touched)

## 📚 Dependencies and Libraries

### Core Libraries

```ini
lib_deps =
    thingpulse/ESP8266 and ESP32 OLED driver for SSD1306 displays@^4.4.0
    sandeepmistry/LoRa@^0.8.0
    me-no-dev/ESPAsyncWebServer
    me-no-dev/AsyncTCP
    bblanchon/ArduinoJson@^6.21.3
```

### Version Pinning

**⚠️ Don't update libraries without testing!**

ESP32 embedded code often breaks with library updates. If you must:

1. Test first on dev branch
2. Check library release notes
3. Test all functions (WiFi, NTRIP, LoRa, Web)
4. Commit change with description "Update library X from Y to Z - tested OK"

## 🔐 Security Best Practices

### Credentials

All credentials (WiFi, NTRIP) are now managed via:

- Web interface configuration
- LittleFS persistent storage
- No hardcoded credentials in code

```cpp
// GOOD: Configuration from ConfigManager
SystemConfig config = configManager.getConfig();
WiFi.begin(config.wifiSSID.c_str(), config.wifiPassword.c_str());

// BAD: Hardcoded credentials
#define WIFI_SSID "MyNetwork"      // NEVER do this!
#define WIFI_PASSWORD "secret123"  // NEVER do this!
```

### Web Interface

```cpp
// Access Point (fallback mode)
const char* AP_SSID = "RTCM_Transmitter_XXXXXX"; // XXXXXX = chip ID
const char* AP_PASSWORD = "rtcm1234";             // Change in production!

// IMPORTANT: In production add:
// - Rate limiting for API
// - HTTPS for sensitive endpoints
// - Authentication token for REST API
```

## 📖 Documentation

### When to Update Documentation?

**Always** when:

- Adding new feature → Update README.md
- Changing API/interface → Update corresponding .h header
- Changing LoRa protocol → Update LORA_PROTOCOL.md
- Changing configuration → Update README.md Configuration section
- Adding new module → Add description in README.md Architecture section

### Comment Format

```cpp
/**
 * @brief Brief function description
 *
 * Longer description if needed - what the function does,
 * its assumptions, edge cases, etc.
 *
 * @param data Pointer to RTCM data
 * @param length Data length in bytes
 * @return true if transmission successful, false otherwise
 */
bool transmitMessage(const uint8_t* data, size_t length);
```

## ⚠️ Common Pitfalls

### 1. Don't transmit in STATE_GATHERING!

```cpp
// BAD!
void loop() {
    loraTransmitter->transmitMessage(data, len); // Not checking state!
}

// GOOD
void loop() {
    if (currentState == STATE_TRANSMISSION) {
        loraTransmitter->transmitMessage(data, len);
    }
}
```

### 2. Watchdog Timeout

ESP32 has watchdog - if you block too long, system will restart.

```cpp
// BAD: Long operation without yield
for (int i = 0; i < 10000; i++) {
    processData(i);
}

// GOOD: Yield periodically
for (int i = 0; i < 10000; i++) {
    processData(i);
    if (i % 100 == 0) {
        yield(); // or delay(1)
    }
}
```

### 3. Memory Leaks

```cpp
// BAD: Allocation in loop() without free
void loop() {
    uint8_t* buffer = new uint8_t[1024]; // Leak!
    processBuffer(buffer);
    // No delete[]!
}

// GOOD: Stack allocation
void loop() {
    uint8_t buffer[1024];
    processBuffer(buffer);
    // Automatically deallocated
}
```

### 4. F() Macro for String Literals

```cpp
// BAD: String literal in RAM
Serial.println("This uses precious RAM!");

// GOOD: F() macro stores in flash
Serial.println(F("This is stored in flash!"));
```

## 🤖 AI Agent Specific Guidelines

### For GitHub Copilot / Claude / Cursor

1. **Always read context** - Don't guess, use `read_file` on relevant files
2. **Plan before action** - Create todo list before any changes
3. **Small changes** - One feature = several small commits, not one huge diff
4. **Test after each change** - Suggest user test after each change
5. **Follow existing patterns** - Don't introduce new styles if there's an established pattern

### If You're Not Sure

**Ask the user** instead of guessing:

- "Should this change work in both states (TX and GATHERING)?"
- "Should I add this option to web interface too?"
- "Should I update the README documentation?"

### Context Gathering

Before a major change, read:

- `src/main.cpp` - State machine logic
- `include/config.h` - Current parameters
- Relevant module `.h` and `.cpp`
- `LORA_PROTOCOL.md` if changes involve radio

## 📞 Questions?

If this document doesn't answer your question:

1. Check inline comments in code
2. Read README.md
3. Check LORA_PROTOCOL.md for protocol details
4. Open a GitHub Issue with your question

---

**Last Update**: January 2026  
**Maintainer**: OSS Transmitter Team

**Remember**: Good code is code that the next developer (or you in 6 months) can understand in 5 minutes! 🚀
