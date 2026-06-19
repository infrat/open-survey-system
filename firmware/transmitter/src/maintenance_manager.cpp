#include "maintenance_manager.h"
#include <LoRa.h>

// ==================== TLV tag constants ====================

#define MAINT_TLV_FREE_HEAP 0x02 // uint32 LE — free heap bytes

// ==================== Constructor / Destructor ====================

MaintenanceManager::MaintenanceManager(ConfigManager &configMgr,
                                       volatile SystemState *statePtr)
    : configManager(configMgr), statePtr(statePtr), lastMaintenanceTime(0),
      activeWindowStartTime(0), deferredStopTriggered(false),
      deferredStopCancelled(false), inStandbyRxMode(false)
{
    telemetryMutex = xSemaphoreCreateMutex();

    // Built-in TLV provider: free heap
    registerTLVField(MAINT_TLV_FREE_HEAP, [this](std::vector<uint8_t> &buf) {
        uint32_t heap = telemetry.free_heap; // read under tick() — mutex already taken
        buf.push_back(MAINT_TLV_FREE_HEAP);
        buf.push_back(4);
        buf.push_back((heap >> 0) & 0xFF);
        buf.push_back((heap >> 8) & 0xFF);
        buf.push_back((heap >> 16) & 0xFF);
        buf.push_back((heap >> 24) & 0xFF);
    });
}

MaintenanceManager::~MaintenanceManager()
{
    if (telemetryMutex)
        vSemaphoreDelete(telemetryMutex);
}

// ==================== Public API ====================

bool MaintenanceManager::isEnabled() const
{
    return configManager.getConfig().maintenance.enabled;
}

void MaintenanceManager::cancelDeferredStop()
{
    deferredStopCancelled = true;
#ifdef DEBUG
    Serial.println(F("[MAINT] Deferred stop cancelled by CMD_START — TX window disabled"));
#endif
}

void MaintenanceManager::registerCommand(MaintenanceCommand cmd,
                                         CommandHandler handler)
{
    commandHandlers[static_cast<uint8_t>(cmd)] = handler;
}

void MaintenanceManager::registerTLVField(
    uint8_t tag, std::function<void(std::vector<uint8_t> &)> provider)
{
    tlvProviders[tag] = provider;
}

void MaintenanceManager::updateTelemetry(const MaintenanceTelemetry &t)
{
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(5)) == pdTRUE)
    {
        telemetry = t;
        xSemaphoreGive(telemetryMutex);
    }
}

void MaintenanceManager::tick()
{
    // In standby, tickStandby() owns the LoRa radio and the maintenance cycle.
    // tick() must not touch state or transmit anything while standby is active.
    if (*statePtr == STATE_STANDBY)
        return;

    const SystemConfig &cfg = configManager.getConfig();

    // --- Deferred stop (requires maintenance.enabled — same channel for CMD_START) ---
    // Only active when not cancelled by a manual CMD_START
    if (cfg.maintenance.enabled && cfg.maintenance.deferredStopEnabled && !deferredStopCancelled && *statePtr == STATE_RUNNING)
    {
        // Reset timer each time we (re-)enter STATE_RUNNING after a stop
        if (activeWindowStartTime == 0 || deferredStopTriggered)
        {
            deferredStopTriggered = false;
            activeWindowStartTime = millis();
        }
        if (millis() - activeWindowStartTime >= cfg.maintenance.activeWindowMs)
        {
            deferredStopTriggered = true;
#ifdef DEBUG
            Serial.println(F("[MAINT] Active TX window expired — entering standby"));
#endif
            *statePtr = STATE_STANDBY;
            return;
        }
    }

    // --- Periodic maintenance frame ---
    if (!isEnabled())
        return;
    if (millis() - lastMaintenanceTime < cfg.maintenance.intervalMs)
        return;

    lastMaintenanceTime = millis();

    // --- Phase 1: TX maintenance frame ---
    if (statePtr)
        *statePtr = STATE_MAINTENANCE_TX;

    sendMaintenanceFrame();

    // --- Phase 2: RX listen window (only if rxWindowEnabled) ---
    if (cfg.maintenance.rxWindowEnabled)
    {
        if (statePtr)
            *statePtr = STATE_MAINTENANCE_RX;
        listenForCommands(cfg.maintenance.listenDurationMs);
    }

    // --- Done: resume normal operation ---
    if (statePtr)
        *statePtr = STATE_RUNNING;
}

// ==================== Public: standby tick (Core 0) ====================

void MaintenanceManager::tickStandby()
{
    // Guard: only operate when truly in standby
    if (*statePtr != STATE_STANDBY && *statePtr != STATE_MAINTENANCE_TX)
    {
        if (inStandbyRxMode)
        {
            LoRa.idle();
            inStandbyRxMode = false;
        }
        return;
    }

    // Enable continuous RX on first call after entering standby
    if (!inStandbyRxMode)
    {
        LoRa.receive();
        inStandbyRxMode = true;
#ifdef DEBUG
        Serial.println(F("[MAINT] Standby: continuous RX enabled"));
#endif
    }

    // Send periodic maintenance frame if enabled and due (TX → back to RX)
    const SystemConfig &cfg = configManager.getConfig();
    if (isEnabled() && millis() - lastMaintenanceTime >= cfg.maintenance.intervalMs)
    {
        lastMaintenanceTime = millis();
        LoRa.idle();
        inStandbyRxMode = false;
        *statePtr = STATE_MAINTENANCE_TX;
        sendMaintenanceFrame();
        *statePtr = STATE_STANDBY;
        // RX will be re-enabled on next call
        return;
    }

    // Poll for incoming command frames (non-blocking)
    int packetSize = LoRa.parsePacket();
    if (packetSize > 0)
    {
        uint8_t buf[250];
        size_t i = 0;
        while (LoRa.available() && i < sizeof(buf))
            buf[i++] = static_cast<uint8_t>(LoRa.read());

        if (i >= 6 && buf[0] == FRAME_TYPE_COMMAND)
        {
#ifdef DEBUG
            Serial.printf("[MAINT] Standby: command frame received (%d bytes)\n", i);
#endif
            dispatchCommand(buf + 5, buf[4]);

            // If a handler changed state out of standby, clean up RX mode
            if (*statePtr != STATE_STANDBY)
            {
                LoRa.idle();
                inStandbyRxMode = false;
            }
        }
    }
}

// ==================== Private: build payload ====================

std::vector<uint8_t> MaintenanceManager::buildPayload()
{
    // Snapshot telemetry under mutex
    MaintenanceTelemetry t;
    if (xSemaphoreTake(telemetryMutex, pdMS_TO_TICKS(10)) == pdTRUE)
    {
        t = telemetry;
        xSemaphoreGive(telemetryMutex);
    }

    std::vector<uint8_t> buf;
    buf.reserve(32);

    // --- Fixed header: 18 bytes ---

    // uptime_s (uint32 LE)
    buf.push_back((t.uptime_s >> 0) & 0xFF);
    buf.push_back((t.uptime_s >> 8) & 0xFF);
    buf.push_back((t.uptime_s >> 16) & 0xFF);
    buf.push_back((t.uptime_s >> 24) & 0xFF);

    // battery_mv (uint16 LE)
    buf.push_back((t.battery_mv >> 0) & 0xFF);
    buf.push_back((t.battery_mv >> 8) & 0xFF);

    // rssi (int8) and snr (int8)
    buf.push_back(static_cast<uint8_t>(t.last_rssi));
    buf.push_back(static_cast<uint8_t>(t.last_snr));

    // rtcm_rx_total (uint32 LE)
    buf.push_back((t.rtcm_rx_total >> 0) & 0xFF);
    buf.push_back((t.rtcm_rx_total >> 8) & 0xFF);
    buf.push_back((t.rtcm_rx_total >> 16) & 0xFF);
    buf.push_back((t.rtcm_rx_total >> 24) & 0xFF);

    // rtcm_tx_total (uint32 LE)
    buf.push_back((t.rtcm_tx_total >> 0) & 0xFF);
    buf.push_back((t.rtcm_tx_total >> 8) & 0xFF);
    buf.push_back((t.rtcm_tx_total >> 16) & 0xFF);
    buf.push_back((t.rtcm_tx_total >> 24) & 0xFF);

    // flags (uint8)
    uint8_t flags = 0;
    if (t.ntrip_connected) flags |= (1 << 0);
    if (t.wifi_connected)  flags |= (1 << 1);
    if (t.battery_mv > 0)  flags |= (1 << 2);
    buf.push_back(flags);

    // reserved
    buf.push_back(0x00);

    // --- TLV extension block ---
    // tlvProviders is read on Core 0 only (set up in setup() before task start)
    for (auto &kv : tlvProviders)
        kv.second(buf);

    return buf;
}

// ==================== Private: send maintenance frame ====================

void MaintenanceManager::sendMaintenanceFrame()
{
    std::vector<uint8_t> payload = buildPayload();

    // Cap payload so the full packet fits in 250 bytes
    // Packet layout: [frameType 1B][msgId 1B][totalPkts 1B][pktNum 1B][dataLen 1B][data]
    if (payload.size() > 245)
        payload.resize(245);

    uint8_t packet[250];
    packet[0] = FRAME_TYPE_MAINTENANCE;
    packet[1] = 0x00; // msgId — not meaningful for maintenance frames
    packet[2] = 0x01; // totalPackets = 1
    packet[3] = 0x00; // packetNumber = 0
    packet[4] = static_cast<uint8_t>(payload.size());
    memcpy(packet + 5, payload.data(), payload.size());

#ifdef DEBUG
    Serial.printf("[MAINT] Sending maintenance frame (%d payload bytes)\n", payload.size());
#endif

    LoRa.beginPacket();
    LoRa.write(packet, 5 + payload.size());
    LoRa.endPacket();
}

// ==================== Private: RX listen window ====================

void MaintenanceManager::listenForCommands(uint32_t durationMs)
{
#ifdef DEBUG
    Serial.printf("[MAINT] Entering RX mode for %u ms\n", durationMs);
#endif

    LoRa.receive(); // Switch SX1276 to continuous RX mode

    unsigned long deadline = millis() + durationMs;
    while (millis() < deadline)
    {
        int packetSize = LoRa.parsePacket();
        if (packetSize > 0)
        {
            uint8_t buf[250];
            size_t i = 0;
            while (LoRa.available() && i < sizeof(buf))
                buf[i++] = static_cast<uint8_t>(LoRa.read());

            // Validate: must start with FRAME_TYPE_COMMAND and have a full 5-byte header
            if (i >= 6 && buf[0] == FRAME_TYPE_COMMAND)
            {
#ifdef DEBUG
                Serial.printf("[MAINT] Command frame received (%d bytes)\n", i);
#endif
                // buf[4] = dataLength; command payload starts at buf[5]
                dispatchCommand(buf + 5, buf[4]);
            }
        }
        delay(1); // yield to FreeRTOS / feed watchdog
    }

    LoRa.idle(); // Return to standby; next beginPacket() re-activates TX

#ifdef DEBUG
    Serial.println(F("[MAINT] RX window closed"));
#endif
}

// ==================== Private: dispatch command ====================

void MaintenanceManager::dispatchCommand(const uint8_t *data, size_t len)
{
    if (len < 2)
        return;

    uint8_t cmdId     = data[0];
    uint8_t payloadLen = data[1];

    // Whitelist check
    const SystemConfig &cfg = configManager.getConfig();
    bool allowed = false;
    for (uint8_t id : cfg.maintenance.enabledCommands)
    {
        if (id == cmdId)
        {
            allowed = true;
            break;
        }
    }

    if (!allowed)
    {
#ifdef DEBUG
        Serial.printf("[MAINT] Command 0x%02X not in whitelist — ignored\n", cmdId);
#endif
        return;
    }

    auto it = commandHandlers.find(cmdId);
    if (it == commandHandlers.end())
    {
#ifdef DEBUG
        Serial.printf("[MAINT] No handler for command 0x%02X\n", cmdId);
#endif
        return;
    }

    const uint8_t *payload = (len >= static_cast<size_t>(2 + payloadLen))
                                 ? data + 2
                                 : nullptr;
    uint8_t actualLen = (payload != nullptr) ? payloadLen : 0;

#ifdef DEBUG
    Serial.printf("[MAINT] Dispatching command 0x%02X (%d payload bytes)\n",
                  cmdId, actualLen);
#endif

    it->second(payload, actualLen);
}
