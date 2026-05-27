#include <Arduino.h>
#include <WiFi.h>
#include <LoRa.h>
#include <map>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "config.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "ntrip_client.h"
#include "rtcm_parser.h"
#include "lora_transmitter.h"
#include "display_manager.h"
#include "web_interface_manager.h"
#include "system_state.h"
#include "maintenance_manager.h"

// ==================== RTCM Per-Type Buffer (Core 1 → Core 0) ====================

struct RTCMBufferItem
{
    static const size_t MAX_SIZE = 1029; // Max RTCM3 message (1023 data + 3 header + 3 CRC)
    uint8_t data[MAX_SIZE];
    size_t length;
    uint16_t messageType;
    unsigned long timestamp; // millis() when received
};

// Per-type buffer: always keeps the latest message of each type
// Protected by mutex for cross-core access
std::map<uint16_t, RTCMBufferItem> rtcmBuffer;
SemaphoreHandle_t rtcmBufferMutex = nullptr;
TaskHandle_t loraTaskHandle = nullptr;

// Overwrite counter (for stats)
volatile uint32_t totalRTCMOverwritten = 0;

// ==================== GLOBAL OBJECTS ====================

ConfigManager configManager;
WiFiManager *wifiManager = nullptr;
NTRIPClient *ntripClient = nullptr;
RTCMParser *rtcmParser = nullptr;
LoRaTransmitter *loraTransmitter = nullptr;
DisplayManager displayManager;
WebInterfaceManager *webInterface = nullptr;
MaintenanceManager *maintenanceManager = nullptr;

// ==================== STATE ====================

volatile SystemState currentState = STATE_INIT;

// ==================== STATISTICS ====================

// RX counters (written only by Core 1)
unsigned long totalRTCMFramesReceived = 0;
uint32_t rtcmRxLastSecond = 0;
uint32_t rtcmRxCurrentSecond = 0;

// TX counters (written only by Core 0, read by Core 1 for display)
volatile unsigned long totalRTCMFramesTransmitted = 0;
volatile unsigned long totalBytesTransmitted = 0;
volatile uint32_t rtcmTxLastSecond = 0;
volatile uint32_t rtcmTxCurrentSecond = 0;

// Display update tracking
unsigned long lastDisplayUpdate = 0;
#define DISPLAY_UPDATE_INTERVAL 500
unsigned long lastSecondTimestamp = 0;
unsigned long lastStatsUpdate = 0;

// ==================== HARDWARE PINS ====================

#define LED_PIN 4 // T-Beam v1.1 uses GPIO 4 for the blue LED

// ==================== Priority Lookup ====================

// Fast lookup table for priority types (populated from config at startup)
bool isPriorityType[4096] = {false};

void initPriorityLookup()
{
    memset(isPriorityType, false, sizeof(isPriorityType));
    SystemConfig &cfg = configManager.getConfig();
    for (uint16_t type : cfg.rtcmPriorityMessageTypes)
    {
        if (type < 4096)
            isPriorityType[type] = true;
    }

    // 1005 (station position) is always priority — critical for RTK solution
    isPriorityType[1005] = true;
}

// ==================== LoRa TX Task (Core 0) ====================
// Snapshot approach: take entire buffer atomically, transmit priority first,
// then low-priority only if no fresh priority data has arrived.
// Every LOW_PRIORITY_INTERVAL_MS, force low-priority through.

#define LOW_PRIORITY_INTERVAL_MS 5000

void loraTask(void *parameter)
{
    std::map<uint16_t, RTCMBufferItem> batch;
    unsigned long lastLowPriorityTime = millis();

#ifdef DEBUG
    Serial.println(F("[LoRa Task] Started on Core 0 (snapshot mode)"));
#endif

    while (true)
    {
        // Maintenance frame cycle (Core 0 — safe to call LoRa.*)
        maintenanceManager->tick();

        // Standby mode: RTCM TX halted, LoRa in continuous RX
        if (currentState == STATE_STANDBY)
        {
            maintenanceManager->tickStandby();
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        if (currentState == STATE_ERROR)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // Atomic snapshot: swap entire buffer
        bool gotData = false;
        if (xSemaphoreTake(rtcmBufferMutex, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            if (!rtcmBuffer.empty())
            {
                std::swap(batch, rtcmBuffer);
                gotData = true;
            }
            xSemaphoreGive(rtcmBufferMutex);
        }

        if (!gotData)
        {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Pass 1: Transmit ALL priority items (MSM + 1005 + 1230)
        // std::map iterates by key ascending: 1005 goes first (small, fast)
        for (auto it = batch.begin(); it != batch.end(); ++it)
        {
            if (!isPriorityType[it->first])
                continue;

            bool success = loraTransmitter->transmitMessage(it->second.data, it->second.length);
            if (success)
            {
                totalRTCMFramesTransmitted++;
                totalBytesTransmitted += it->second.length;
                rtcmTxCurrentSecond++;
#ifdef DEBUG
                Serial.printf("✓ RTCM %d (%d bytes) [PRI]\n", it->first, it->second.length);
#endif
            }
        }

        // Pass 2: Transmit low-priority items (ephemeris, antenna descriptors)
        // Skip if new priority data has arrived AND we've sent low-priority recently
        bool forceLow = (millis() - lastLowPriorityTime >= LOW_PRIORITY_INTERVAL_MS);
        bool skipLow = false;

        if (!forceLow)
        {
            if (xSemaphoreTake(rtcmBufferMutex, pdMS_TO_TICKS(10)) == pdTRUE)
            {
                for (auto it = rtcmBuffer.begin(); it != rtcmBuffer.end(); ++it)
                {
                    if (isPriorityType[it->first])
                    {
                        skipLow = true;
                        break;
                    }
                }
                xSemaphoreGive(rtcmBufferMutex);
            }
        }

        if (!skipLow)
        {
            for (auto it = batch.begin(); it != batch.end(); ++it)
            {
                if (isPriorityType[it->first])
                    continue; // already sent in pass 1

                bool success = loraTransmitter->transmitMessage(it->second.data, it->second.length);
                if (success)
                {
                    totalRTCMFramesTransmitted++;
                    totalBytesTransmitted += it->second.length;
                    rtcmTxCurrentSecond++;
#ifdef DEBUG
                    Serial.printf("✓ RTCM %d (%d bytes) [LOW]\n", it->first, it->second.length);
#endif
                }
            }
            lastLowPriorityTime = millis();
        }
#ifdef DEBUG
        else
        {
            Serial.println(F("⏭ Skipping low-priority, fresh MSM data waiting"));
        }
#endif

        batch.clear();
    }
}

// ==================== SETUP ====================

void setup()
{
    Serial.begin(115200);
#ifdef DEBUG
    Serial.println(F("\n========================================"));
    Serial.println(F("  TTGO T-Beam LoRa RTCM Transmitter"));
    Serial.println(F("  Dual-Core Architecture"));
    Serial.println(F("========================================"));
#endif

    // Initialize LED pin
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH);

#ifdef DEBUG
    // Disable watchdog timer for debugging
    disableCore0WDT();
    disableCore1WDT();
    Serial.println(F("Watchdog timers disabled for debugging"));
#endif

    // Initialize display
    displayManager.begin();
    displayManager.showSplashScreen();
    delay(3000);

    displayManager.showStatus("Config", "Loading...");

    // Initialize ConfigManager
    if (!configManager.begin())
    {
#ifdef DEBUG
        Serial.println(F("ConfigManager initialization failed!"));
#endif
        displayManager.showError("Config Failed");
        delay(5000);
        // Continue with defaults
    }

    // Create manager objects with ConfigManager
    wifiManager = new WiFiManager(configManager);
    ntripClient = new NTRIPClient(configManager);
    rtcmParser = new RTCMParser(configManager);
    loraTransmitter = new LoRaTransmitter(configManager);
    webInterface = new WebInterfaceManager(configManager, *ntripClient);
    maintenanceManager = new MaintenanceManager(configManager, &currentState);

    // Register maintenance command handlers (extensible — add more here)
    maintenanceManager->registerCommand(MaintenanceCommand::CMD_RESET,
        [](const uint8_t *, uint8_t) {
            Serial.println(F("[MAINT] Remote reset triggered"));
            delay(500);
            ESP.restart();
        });
    maintenanceManager->registerCommand(MaintenanceCommand::CMD_PING,
        [](const uint8_t *, uint8_t) {
            Serial.println(F("[MAINT] Ping received"));
        });
    maintenanceManager->registerCommand(MaintenanceCommand::CMD_SLEEP,
        [](const uint8_t *p, uint8_t len) {
            if (len >= 4) {
                uint32_t secs;
                memcpy(&secs, p, 4);
                Serial.printf("[MAINT] Deep sleep for %u s\n", secs);
                esp_sleep_enable_timer_wakeup((uint64_t)secs * 1000000ULL);
                esp_deep_sleep_start();
            }
        });
    maintenanceManager->registerCommand(MaintenanceCommand::CMD_LOGLEVEL,
        [](const uint8_t *p, uint8_t len) {
            if (len >= 1)
                Serial.printf("[MAINT] Log level set to %d\n", p[0]);
        });
    maintenanceManager->registerCommand(MaintenanceCommand::CMD_START,
        [](const uint8_t *, uint8_t) {
            Serial.println(F("[MAINT] CMD_START received — resuming RTCM transmission"));
            maintenanceManager->cancelDeferredStop(); // disable automatic TX window for this session
            currentState = STATE_RUNNING;
        });
    maintenanceManager->registerCommand(MaintenanceCommand::CMD_STOP,
        [](const uint8_t *, uint8_t) {
            Serial.println(F("[MAINT] CMD_STOP received — entering standby"));
            currentState = STATE_STANDBY; // tickStandby() will enable RX on next call
        });

    // Update RTCM message types from config
    rtcmParser->updateMessageTypes();

    // Initialize priority lookup table
    initPriorityLookup();

    displayManager.showStatus("Starting...", "Initializing system");

    // Start Web Interface (AP mode)
    webInterface->begin();

    // Initialize LoRa
    if (!loraTransmitter->begin())
    {
#ifdef DEBUG
        Serial.println(F("LoRa initialization failed!"));
#endif
        displayManager.showError("LoRa Init Failed");
        currentState = STATE_ERROR;
        while (1)
            delay(1000);
    }

    // Connect to WiFi
    displayManager.showStatus("WiFi", "Connecting...");
    if (!wifiManager->connect())
    {
#ifdef DEBUG
        Serial.println(F("WiFi connection failed!"));
#endif
        displayManager.showError("WiFi Failed");
        currentState = STATE_ERROR;
        while (1)
            delay(1000);
    }

    // Connect to NTRIP
    displayManager.showStatus("NTRIP", "Connecting...");
    if (!ntripClient->connect())
    {
#ifdef DEBUG
        Serial.println(F("NTRIP connection failed!"));
#endif
        displayManager.showError("NTRIP Failed");
        currentState = STATE_ERROR;
        while (1)
            delay(1000);
    }

#ifdef DEBUG
    Serial.println(F("NTRIP connected successfully"));
#endif

    // Create mutex for per-type RTCM buffer
    rtcmBufferMutex = xSemaphoreCreateMutex();
    if (rtcmBufferMutex == nullptr)
    {
#ifdef DEBUG
        Serial.println(F("Failed to create RTCM buffer mutex!"));
#endif
        displayManager.showError("Mutex Failed");
        currentState = STATE_ERROR;
        while (1)
            delay(1000);
    }

#ifdef DEBUG
    Serial.println(F("[INFO] RTCM per-type buffer with mutex created"));
#endif

    // Setup RTCM callback — store latest message per type for Core 0
    rtcmParser->setCallback([](const uint8_t *data, size_t length, uint16_t messageType)
                            {
        totalRTCMFramesReceived++;
        rtcmRxCurrentSecond++;

        if (!rtcmParser->shouldTransmitMessageType(messageType))
        {
            return;
        }

        if (length > RTCMBufferItem::MAX_SIZE)
        {
#ifdef DEBUG
            Serial.printf("⚠ RTCM %d too large (%d bytes)\n", messageType, length);
#endif
            return;
        }

        RTCMBufferItem item;
        memcpy(item.data, data, length);
        item.length = length;
        item.messageType = messageType;
        item.timestamp = millis();

        if (xSemaphoreTake(rtcmBufferMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            bool existed = rtcmBuffer.count(messageType) > 0;
            rtcmBuffer[messageType] = item;
            if (existed)
            {
                totalRTCMOverwritten++;
            }
            xSemaphoreGive(rtcmBufferMutex);
#ifdef DEBUG
            Serial.printf("⬇ RTCM %d (%d bytes) %s\n", messageType, length,
                          existed ? "updated" : "buffered");
#endif
        } });

    // Start LoRa TX task on Core 0
    xTaskCreatePinnedToCore(
        loraTask,        // Task function
        "LoRaTX",        // Task name
        8192,            // Stack size (bytes)
        nullptr,         // Parameter
        1,               // Priority
        &loraTaskHandle, // Task handle
        0                // Core 0 (PRO_CPU)
    );

#ifdef DEBUG
    Serial.println(F("\n[INFO] Dual-core system initialized:"));
    Serial.println(F("  Core 1: NTRIP RX, Parser, WiFi, Web, Display"));
    Serial.println(F("  Core 0: LoRa TX (FreeRTOS task)"));
    Serial.println(F("[INFO] Waiting for RTCM data..."));
#endif

    currentState = STATE_RUNNING;
    lastSecondTimestamp = millis();

    displayManager.showStatus("Running", "Dual-core active");
    delay(1000);
}

// ==================== MAIN LOOP (Core 1) ====================

void loop()
{
    yield(); // Feed watchdog

    // Update web interface (handles AP timeout)
    webInterface->loop();

    // In standby mode: skip WiFi/NTRIP processing, Core 1 is mostly idle
    if (currentState == STATE_STANDBY)
    {
        if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
        {
            displayManager.updateStatsDisplay(currentState, 0, 0,
                                              wifiManager->isConnected(), false);
            lastDisplayUpdate = millis();
        }
        delay(100);
        return;
    }

    // Check WiFi connection
    if (!wifiManager->isConnected())
    {
#ifdef DEBUG
        Serial.println(F("[ERROR] WiFi disconnected!"));
#endif
        displayManager.showStatus("WiFi", "Reconnecting...");
        if (!wifiManager->connect())
        {
            delay(5000);
            return;
        }
    }

    // Check NTRIP connection
    if (!ntripClient->isConnected())
    {
#ifdef DEBUG
        Serial.println(F("[ERROR] NTRIP disconnected!"));
#endif
        displayManager.showStatus("NTRIP", "Reconnecting...");
        if (!ntripClient->connect())
        {
            delay(5000);
            return;
        }
    }

    // Read NTRIP data and feed to parser (non-blocking — LoRa TX is on Core 0)
    size_t available = ntripClient->available();
    if (available > 0)
    {
        uint8_t buffer[512];
        size_t maxRead = min(available, sizeof(buffer));
        size_t bytesRead = ntripClient->read(buffer, maxRead);
        rtcmParser->processData(buffer, bytesRead);
    }

    // Update OLED display periodically
    if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
    {
        // Calculate msg/s every second
        if (millis() - lastSecondTimestamp >= 1000)
        {
            rtcmRxLastSecond = rtcmRxCurrentSecond;
            rtcmRxCurrentSecond = 0;
            rtcmTxLastSecond = rtcmTxCurrentSecond;
            rtcmTxCurrentSecond = 0;
            lastSecondTimestamp = millis();
        }

        displayManager.updateStatsDisplay(currentState, rtcmRxLastSecond, rtcmTxLastSecond,
                                          wifiManager->isConnected(), ntripClient->isConnected());
        lastDisplayUpdate = millis();

        // Push telemetry snapshot for maintenance frame (safe — Core 1 side)
        MaintenanceTelemetry tel;
        tel.uptime_s        = millis() / 1000;
        tel.rtcm_rx_total   = totalRTCMFramesReceived;
        tel.rtcm_tx_total   = totalRTCMFramesTransmitted;
        tel.ntrip_connected = ntripClient->isConnected();
        tel.wifi_connected  = wifiManager->isConnected();
        tel.free_heap       = esp_get_free_heap_size();
        // Battery: raw ADC → mV using voltage divider constant from config.h
        uint16_t rawAdc = analogRead(BATTERY_ADC_PIN);
        tel.battery_mv = static_cast<uint16_t>((rawAdc / 4095.0f) * 3300.0f * BATTERY_SCALE_FACTOR);
        maintenanceManager->updateTelemetry(tel);
    }

#ifdef DEBUG
    // Print stats every 30 seconds
    if (millis() - lastStatsUpdate >= 30000)
    {
        Serial.printf("\n=== Statistics ===\n");
        Serial.printf("State: %s\n", currentState == STATE_RUNNING ? "RUNNING" : "ERROR");
        Serial.printf("RTCM RX: %lu total, %u msg/s\n", totalRTCMFramesReceived, rtcmRxLastSecond);
        Serial.printf("RTCM TX: %lu total, %u msg/s (%lu bytes)\n",
                      totalRTCMFramesTransmitted, rtcmTxLastSecond, totalBytesTransmitted);
        size_t bufferSize = 0;
        if (xSemaphoreTake(rtcmBufferMutex, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            bufferSize = rtcmBuffer.size();
            xSemaphoreGive(rtcmBufferMutex);
        }
        Serial.printf("Buffer: %d types pending, %lu overwrites\n", bufferSize, (unsigned long)totalRTCMOverwritten);
        Serial.printf("WiFi: %s, NTRIP: %s\n",
                      wifiManager->isConnected() ? "OK" : "ERR",
                      ntripClient->isConnected() ? "OK" : "ERR");
        Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
        Serial.printf("==================\n\n");

        lastStatsUpdate = millis();
    }
#endif

    // Keep NTRIP connection alive — only when actively transmitting RTCM
    if (currentState == STATE_RUNNING)
    {
        ntripClient->keepAlive();
    }
}