#include "wifi_manager.h"
#include <WiFi.h>
#include <esp_system.h>
#include "logger.h"

String WifiManager::defaultApSsid()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA); // works before WiFi is started
    char name[32];
    snprintf(name, sizeof(name), AP_SSID_PREFIX "%02X%02X", mac[4], mac[5]);
    return String(name);
}

void WifiManager::begin(const RoverSettings &s)
{
    _mode = s.wifiMode;
    _staSsid = s.staSsid;
    _staPassword = s.staPassword;
    _apSsid = s.apSsid.length() ? s.apSsid : defaultApSsid();
    _apPassword = s.apPassword;
    _hasAp = _mode != WifiMode::STA;
    // AP+STA with no network configured is simply an AP
    _hasSta = _mode != WifiMode::AP && _staSsid.length() > 0;
    _alwaysOn = s.wifiAlwaysOn();
    _windowMs = (uint32_t)s.setupWindowSec * 1000UL;

    WiFi.persistent(false); // settings live in NVS under our control, not in the WiFi driver
    WiFi.setHostname(OTA_HOSTNAME);

    if (_hasAp && _hasSta)
        WiFi.mode(WIFI_AP_STA);
    else if (_hasAp)
        WiFi.mode(WIFI_AP);
    else
        WiFi.mode(WIFI_STA);

    _enabled = true;
    _startMs = millis();
    keepAlive();

    if (_hasAp)
    {
        startAp();
    }
    if (_hasSta)
    {
        logPrintln("[WiFi] Connecting to \"%s\"...", _staSsid.c_str());
        WiFi.setAutoReconnect(true);
        WiFi.begin(_staSsid.c_str(), _staPassword.c_str());
    }

    if (_alwaysOn)
    {
        logPrintln("[WiFi] Stays on (no auto-shutdown)");
    }
    else
    {
        logPrintln("[WiFi] Setup window: off after %u s without web UI / OTA / Telnet use", (unsigned)(_windowMs / 1000UL));
    }
}

void WifiManager::startAp()
{
    bool ok = WiFi.softAP(_apSsid.c_str(), _apPassword.length() ? _apPassword.c_str() : nullptr);
    _apStarted = ok;
    if (ok)
    {
        logPrintln("[WiFi] AP \"%s\" up at %s%s", _apSsid.c_str(), WiFi.softAPIP().toString().c_str(),
                   _apPassword.length() ? "" : " (open)");
    }
    else
    {
        logPrintln("[WiFi] ERROR: could not start AP");
    }
}

bool WifiManager::staConnected() const
{
    return _enabled && _hasSta && WiFi.status() == WL_CONNECTED;
}

bool WifiManager::servicesReady() const
{
    if (!_enabled)
        return false;
    if (staConnected())
        return true;
    if (!_hasSta)
        return _apStarted;
    // STA configured but not connected yet: give it WIFI_TIMEOUT_MS, then
    // serve from the AP if there is one
    return _apStarted && (millis() - _startMs) >= WIFI_TIMEOUT_MS;
}

String WifiManager::staIp() const
{
    return staConnected() ? WiFi.localIP().toString() : String("");
}

String WifiManager::apIp() const
{
    return apRunning() ? WiFi.softAPIP().toString() : String("");
}

uint8_t WifiManager::apClients() const
{
    return apRunning() ? WiFi.softAPgetStationNum() : 0;
}

int32_t WifiManager::remainingSec() const
{
    if (_alwaysOn || !_enabled)
    {
        return -1;
    }
    // Read the activity stamp first: keepAlive() runs on another task, and a
    // stamp newer than "now" would wrap the subtraction into a false timeout.
    uint32_t last = _lastActivityMs;
    uint32_t idle = millis() - last;
    return idle >= _windowMs ? 0 : (int32_t)((_windowMs - idle) / 1000UL);
}

void WifiManager::finishSoon(uint32_t delayMs)
{
    _finishAtMs = millis() + delayMs;
    _finishPending = true;
}

void WifiManager::shutdown(const char *reason)
{
    logPrintln("[WiFi] %s - switching WiFi off", reason);
    logPrintln("[Info] Restart the rover to reach the web UI / OTA / Telnet again");

    if (_onShutdown)
    {
        _onShutdown(); // stop services before the interfaces disappear
    }
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    _enabled = false;
    _apStarted = false;
    _finishPending = false;
}

void WifiManager::loop()
{
    if (!_enabled)
    {
        return;
    }

    if (_hasSta && WiFi.status() == WL_CONNECTED)
    {
        _staEverConnected = true;
    }

    // A station that cannot join its network would leave the rover unreachable
    // (STA mode) or keep dragging the shared radio across channels while it
    // retries, which makes the AP drop packets (AP+STA). Give up and run as a
    // plain AP so the UI stays usable. A network that drops later is different:
    // auto-reconnect handles that.
    if (_hasSta && !_staEverConnected && (millis() - _startMs) >= WIFI_TIMEOUT_MS)
    {
        logPrintln("[WiFi] Could not join \"%s\" - access point only", _staSsid.c_str());
        WiFi.setAutoReconnect(false);
        WiFi.disconnect();
        WiFi.mode(WIFI_AP);
        _hasSta = false;
        if (!_apStarted)
        {
            startAp();
        }
    }

    // The window should not burn down while nobody can reach the rover yet
    if (!_windowArmed && servicesReady())
    {
        _windowArmed = true;
        keepAlive();
    }

    if (_alwaysOn)
    {
        return;
    }

    if (_finishPending && (int32_t)(millis() - _finishAtMs) >= 0)
    {
        shutdown("Setup finished from the web UI");
        return;
    }

    uint32_t last = _lastActivityMs; // before millis(), see remainingSec()
    if ((millis() - last) >= _windowMs)
    {
        shutdown("Setup window closed");
    }
}
