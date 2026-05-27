#ifndef MAINTENANCE_MANAGER_H
#define MAINTENANCE_MANAGER_H

#include <Arduino.h>
#include <functional>
#include <map>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "config_manager.h"
#include "lora_transmitter.h" // for LoRaFrameType
#include "system_state.h"

// ==================== Command IDs ====================

enum class MaintenanceCommand : uint8_t
{
    CMD_RESET     = 0x01, // Restart the device (no payload)
    CMD_SLEEP     = 0x02, // Deep sleep for N seconds (uint32 LE payload)
    CMD_HTTP_GET  = 0x03, // Perform HTTP GET request (null-terminated URL)
    CMD_SET_PARAM = 0x04, // Set a named parameter ("key\0value\0")
    CMD_PING      = 0x05, // Ping — no action, logs reception (no payload)
    CMD_LOGLEVEL  = 0x06, // Change log verbosity (uint8: 0=off, 1=error, 2=debug)
    CMD_START     = 0x07, // Resume RTCM transmission (exit standby)
    CMD_STOP      = 0x08, // Enter standby immediately (RX-only mode)
};

// ==================== Telemetry snapshot ====================

// Filled on Core 1, consumed on Core 0 via mutex.
struct MaintenanceTelemetry
{
    uint32_t uptime_s        = 0;
    uint16_t battery_mv      = 0;   // 0 if not measured
    uint32_t rtcm_rx_total   = 0;
    uint32_t rtcm_tx_total   = 0;
    bool     ntrip_connected = false;
    bool     wifi_connected  = false;
    int8_t   last_rssi       = 0;
    int8_t   last_snr        = 0;
    uint32_t free_heap       = 0;
};

// ==================== Types ====================

using CommandHandler = std::function<void(const uint8_t *payload, uint8_t len)>;

// ==================== MaintenanceManager ====================

class MaintenanceManager
{
public:
    /**
     * @brief Construct a new MaintenanceManager.
     * @param configMgr  Reference to the shared ConfigManager.
     * @param statePtr   Pointer to the volatile global SystemState so the
     *                   manager can switch between STATE_MAINTENANCE_TX /
     *                   STATE_MAINTENANCE_RX / STATE_RUNNING (Core 0 only).
     */
    MaintenanceManager(ConfigManager &configMgr, volatile SystemState *statePtr);
    ~MaintenanceManager();

    /**
     * @brief Must be called from loraTask (Core 0) every iteration.
     *        Checks elapsed time and, when due, sends a maintenance frame
     *        then opens the RX listen window.
     *        Also handles the deferred-stop timer.
     */
    void tick();

    /**
     * @brief Must be called from loraTask (Core 0) when currentState ==
     *        STATE_STANDBY.  Keeps LoRa in continuous RX mode, sends
     *        periodic maintenance frames, and dispatches incoming commands.
     *        Returns immediately (non-blocking) so the caller can loop.
     */
    void tickStandby();

    /**
     * @brief Register a handler for a command ID.  Call from setup() before
     *        the LoRa task starts.  Extensible — any number of commands can
     *        be registered.
     */
    void registerCommand(MaintenanceCommand cmd, CommandHandler handler);

    /**
     * @brief Push a fresh telemetry snapshot.  Call from loop() (Core 1).
     *        Thread-safe via internal mutex.
     */
    void updateTelemetry(const MaintenanceTelemetry &telemetry);

    /**
     * @brief Register a custom TLV field provider.  The provider receives the
     *        payload buffer and should append [tag 1B][len 1B][value lenB].
     *        Call from setup() before the LoRa task starts.
     */
    void registerTLVField(uint8_t tag,
                          std::function<void(std::vector<uint8_t> &)> provider);

    /**
     * @brief Cancel the deferred-stop timer permanently for this session.
     *        Called when CMD_START is received — after a manual start the
     *        active TX window no longer applies until next power cycle.
     */
    void cancelDeferredStop();

    bool isEnabled() const;

private:
    ConfigManager &configManager;
    volatile SystemState *statePtr;

    MaintenanceTelemetry telemetry;
    SemaphoreHandle_t telemetryMutex;

    unsigned long lastMaintenanceTime;

    // Deferred-stop state (Core 0 only)
    unsigned long activeWindowStartTime; // millis() when STATE_RUNNING was first reached
    bool deferredStopTriggered;          // true once the active window has expired
    bool deferredStopCancelled;          // true after CMD_START — timer disabled for session
    bool inStandbyRxMode;                // true while LoRa is in continuous RX (standby)

    std::map<uint8_t, CommandHandler> commandHandlers;
    std::map<uint8_t, std::function<void(std::vector<uint8_t> &)>> tlvProviders;

    // Build fixed-header + TLV payload bytes
    std::vector<uint8_t> buildPayload();

    // Transmit the maintenance frame over LoRa (Core 0)
    void sendMaintenanceFrame();

    // Enter RX mode and wait for a command frame (Core 0)
    void listenForCommands(uint32_t durationMs);

    // Parse a received packet and dispatch to registered handler
    void dispatchCommand(const uint8_t *data, size_t len);
};

#endif // MAINTENANCE_MANAGER_H
