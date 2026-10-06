#include "settings.h"
#include <Preferences.h>

static const char *NVS_NAMESPACE = "rover";

void RoverSettings::setDefaults()
{
    transport = (TransportMode)DEFAULT_TRANSPORT_MODE;
    wifiMode = (WifiMode)DEFAULT_WIFI_MODE;
    staSsid = WIFI_SSID;
    staPassword = WIFI_PASSWORD;
    apSsid = "";
    apPassword = "";
    setupWindowSec = DEFAULT_SETUP_WINDOW_SEC;
    tcpPort = TCP_PORT_DEFAULT;
}

bool RoverSettings::validate(String &error) const
{
    if ((uint8_t)transport > (uint8_t)TransportMode::TCP)
    {
        error = "Unknown transport";
        return false;
    }
    if ((uint8_t)wifiMode > (uint8_t)WifiMode::AP_STA)
    {
        error = "Unknown WiFi mode";
        return false;
    }
    if (wifiMode == WifiMode::STA && staSsid.length() == 0)
    {
        error = "Station mode needs a network name (SSID)";
        return false;
    }
    if (staSsid.length() > 32 || apSsid.length() > 32)
    {
        error = "SSID is longer than 32 characters";
        return false;
    }
    if (staPassword.length() > 63)
    {
        error = "Station password is longer than 63 characters";
        return false;
    }
    if (apPassword.length() != 0 && (apPassword.length() < 8 || apPassword.length() > 63))
    {
        error = "AP password must be 8-63 characters (or empty for an open AP)";
        return false;
    }
    if (setupWindowSec < SETUP_WINDOW_MIN_SEC || setupWindowSec > SETUP_WINDOW_MAX_SEC)
    {
        error = "Setup window must be 15-600 seconds";
        return false;
    }
    if (tcpPort < 1 || tcpPort == HTTP_PORT || tcpPort == TELNET_PORT || tcpPort == 3232)
    {
        error = "TCP port is invalid or used by HTTP, Telnet or OTA";
        return false;
    }
    if (transport == TransportMode::TCP && wifiMode == WifiMode::STA && staSsid.length() == 0)
    {
        error = "WiFi TCP needs a network to join or an access point";
        return false;
    }
    return true;
}

void SettingsStore::load(RoverSettings &s)
{
    s.setDefaults();

    // Opened read-write on purpose: a read-only open fails with NOT_FOUND on
    // the very first boot and spams the log.
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false))
    {
        return;
    }

    s.transport = (TransportMode)prefs.getUChar("tr", (uint8_t)s.transport);
    s.wifiMode = (WifiMode)prefs.getUChar("wm", (uint8_t)s.wifiMode);
    s.staSsid = prefs.getString("ssid", s.staSsid);
    s.staPassword = prefs.getString("pw", s.staPassword);
    s.apSsid = prefs.getString("apssid", s.apSsid);
    s.apPassword = prefs.getString("appw", s.apPassword);
    s.setupWindowSec = prefs.getUShort("setup", s.setupWindowSec);
    s.tcpPort = prefs.getUShort("tcp", s.tcpPort);
    prefs.end();

    String error;
    if (!s.validate(error))
    {
        s.setDefaults();
    }
}

bool SettingsStore::save(const RoverSettings &s)
{
    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false))
    {
        return false;
    }

    bool ok = true;
    ok &= prefs.putUChar("tr", (uint8_t)s.transport) == 1;
    ok &= prefs.putUChar("wm", (uint8_t)s.wifiMode) == 1;
    // putString returns 0 for an empty string, which is a valid value here
    prefs.putString("ssid", s.staSsid);
    prefs.putString("pw", s.staPassword);
    prefs.putString("apssid", s.apSsid);
    prefs.putString("appw", s.apPassword);
    ok &= prefs.putUShort("setup", s.setupWindowSec) == 2;
    ok &= prefs.putUShort("tcp", s.tcpPort) == 2;
    prefs.end();
    return ok;
}

const char *transportModeName(TransportMode mode)
{
    switch (mode)
    {
    case TransportMode::SPP:
        return "SPP";
    case TransportMode::TCP:
        return "TCP";
    default:
        return "BLE";
    }
}

const char *transportModeId(TransportMode mode)
{
    switch (mode)
    {
    case TransportMode::SPP:
        return "spp";
    case TransportMode::TCP:
        return "tcp";
    default:
        return "ble";
    }
}

const char *wifiModeId(WifiMode mode)
{
    switch (mode)
    {
    case WifiMode::STA:
        return "sta";
    case WifiMode::AP_STA:
        return "ap_sta";
    default:
        return "ap";
    }
}
