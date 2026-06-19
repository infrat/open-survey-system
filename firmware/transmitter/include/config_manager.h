#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <vector>
#include "config.h"

// Maintenance frame configuration
struct MaintenanceConfig
{
    bool enabled = false;
    uint32_t intervalMs = 120000;        // 2 minutes between frames
    uint32_t listenDurationMs = 2000;    // 2 seconds RX window after each frame
    bool rxWindowEnabled = false;        // Open RX window after each maintenance frame
    std::vector<uint8_t> enabledCommands; // Whitelisted command IDs (0x01=RESET, 0x05=PING, …)
    // Deferred stop
    bool deferredStopEnabled = false;    // Stop TX after activeWindowMs, enter standby
    uint32_t activeWindowMs = 300000;    // Active TX window duration (default: 5 minutes)
};

// Configuration structure
struct SystemConfig
{
    // WiFi Configuration
    String wifiSSID;
    String wifiPassword;
    uint32_t wifiTimeout;

    // NTRIP Configuration
    String ntripHost;
    uint16_t ntripPort;
    String ntripMountpoint;
    String ntripUser;
    String ntripPassword;
    uint32_t ntripTimeout;

    // GGA Configuration
    double ggaLatitude;
    double ggaLongitude;
    double ggaAltitude;
    uint32_t ggaSendInterval;

    // LoRa Configuration
    uint32_t loraFrequency;
    uint8_t loraSpreadingFactor;
    uint32_t loraBandwidth;
    uint8_t loraCodingRate;
    uint8_t loraTxPower;
    uint8_t loraSyncWord;

    // RTCM Message Type Filtering
    std::vector<uint16_t> rtcmMessageTypes;         // Allowed message types for transmission
    std::vector<uint16_t> rtcmPriorityMessageTypes; // High-priority types (MSM observations)

    // Display Configuration
    uint32_t displayUpdateInterval;

    // Maintenance Frame Configuration
    MaintenanceConfig maintenance;
};

class ConfigManager
{
public:
    ConfigManager();
    bool begin();
    bool loadConfig();
    bool saveConfig();
    void setDefaults();
    SystemConfig &getConfig();
    void printConfig();

private:
    SystemConfig config;
    const char *CONFIG_FILE = "/config.json";
    const size_t JSON_BUFFER_SIZE = 2048;

    bool mountFilesystem();
};

#endif
