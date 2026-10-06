#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include "settings.h"

/**
 * @brief Owns the WiFi radio: AP / STA / AP+STA setup, STA→AP fallback and the
 *        setup window.
 *
 * WiFi and Bluetooth share one radio and most of the heap, so they never run
 * together: after boot WiFi is up alone for the setup window (web UI, OTA,
 * Telnet), then it is switched off for good and main.cpp starts Bluetooth.
 * The window is an idle timeout - keepAlive() restarts it. With the WiFi TCP
 * transport there is no Bluetooth and the radio never times out.
 *
 * Non-blocking: begin() returns immediately, loop() does the follow-up work.
 */
class WifiManager
{
public:
    typedef void (*ShutdownCallback)();

    void begin(const RoverSettings &settings);
    void loop();

    // Restart the idle window. Safe to call from any task.
    void keepAlive() { _lastActivityMs = millis(); }

    // End the window from the UI: WiFi goes off after delayMs (time to answer
    // the request first). Safe to call from any task.
    void finishSoon(uint32_t delayMs);

    // Called once when the window closes, right before the radio is switched off
    void setShutdownCallback(ShutdownCallback callback) { _onShutdown = callback; }

    bool enabled() const { return _enabled; }
    bool alwaysOn() const { return _alwaysOn; }

    // True once the interface OTA/mDNS should bind to is up: STA has an IP, or
    // the AP is up (AP-only mode, or STA failed to connect within the timeout)
    bool servicesReady() const;

    bool staConnected() const;
    bool apRunning() const { return _enabled && _apStarted; }
    String staIp() const;
    String apIp() const;
    const String &apSsid() const { return _apSsid; }
    uint8_t apClients() const;
    WifiMode mode() const { return _mode; }

    // Seconds until the radio is switched off, -1 when it stays on
    int32_t remainingSec() const;

private:
    void startAp();
    void shutdown(const char *reason);
    static String defaultApSsid();

    WifiMode _mode = WifiMode::AP_STA;
    String _staSsid;
    String _staPassword;
    String _apSsid;
    String _apPassword;
    bool _hasSta = false;
    bool _hasAp = false;
    bool _alwaysOn = false;
    uint32_t _windowMs = 0;

    bool _enabled = false;
    bool _apStarted = false;
    bool _staEverConnected = false;
    bool _windowArmed = false; // window restarted once the rover became reachable
    uint32_t _startMs = 0;
    volatile uint32_t _lastActivityMs = 0;
    volatile bool _finishPending = false;
    volatile uint32_t _finishAtMs = 0;
    ShutdownCallback _onShutdown = nullptr;
};

#endif // WIFI_MANAGER_H
