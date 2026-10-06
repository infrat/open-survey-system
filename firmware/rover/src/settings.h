#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include "config.h"

enum class TransportMode : uint8_t
{
    BLE = 0,
    SPP = 1,
    TCP = 2
};

enum class WifiMode : uint8_t
{
    AP = 0,
    STA = 1,
    AP_STA = 2
};

/**
 * @brief Everything the web UI can change. Persisted in NVS.
 */
struct RoverSettings
{
    TransportMode transport;
    WifiMode wifiMode;
    String staSsid;
    String staPassword;
    String apSsid; // empty = auto (OSSRTK-XXXX)
    String apPassword; // empty = open AP
    uint16_t setupWindowSec; // idle seconds before WiFi hands the radio to Bluetooth
    uint16_t tcpPort;

    RoverSettings() { setDefaults(); }

    void setDefaults();

    // false + human readable reason when a value is out of range
    bool validate(String &error) const;

    // TCP clients need the radio, so WiFi never times out there. With a
    // Bluetooth transport WiFi is only up for the setup window after boot.
    bool wifiAlwaysOn() const { return transport == TransportMode::TCP; }
};

class SettingsStore
{
public:
    // Loads from NVS. Falls back to defaults when nothing valid is stored.
    void load(RoverSettings &settings);
    bool save(const RoverSettings &settings);
};

const char *transportModeName(TransportMode mode);
const char *transportModeId(TransportMode mode); // "ble" / "spp" / "tcp"
const char *wifiModeId(WifiMode mode);           // "ap" / "sta" / "ap_sta"

#endif // SETTINGS_H
