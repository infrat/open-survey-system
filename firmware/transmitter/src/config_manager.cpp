#include "config_manager.h"
#include "config.h"

ConfigManager::ConfigManager()
{
    setDefaults();
}

bool ConfigManager::begin()
{
    if (!mountFilesystem())
    {
#ifdef DEBUG
        Serial.println("ConfigManager: Failed to mount LittleFS");
#endif
        return false;
    }

#ifdef DEBUG
    Serial.println("ConfigManager: LittleFS mounted successfully");
    Serial.printf("ConfigManager: Before loadConfig() - WiFi SSID: '%s' (len=%d)\n",
                  config.wifiSSID.c_str(), config.wifiSSID.length());
#endif

    bool result = loadConfig();

#ifdef DEBUG
    Serial.printf("ConfigManager: After loadConfig() - WiFi SSID: '%s' (len=%d)\n",
                  config.wifiSSID.c_str(), config.wifiSSID.length());
#endif

    return result;
}

bool ConfigManager::mountFilesystem()
{
    if (!LittleFS.begin(true)) // format on fail
    {
        return false;
    }
    return true;
}

void ConfigManager::setDefaults()
{
    // WiFi defaults
    config.wifiSSID = "";
    config.wifiPassword = "";
    config.wifiTimeout = 30000;

#ifdef DEBUG
    Serial.printf("ConfigManager: setDefaults() - WiFi SSID: '%s' (len=%d)\n",
                  config.wifiSSID.c_str(), config.wifiSSID.length());
#endif

    // NTRIP defaults
    config.ntripHost = "91.198.76.2";
    config.ntripPort = 2101;
    config.ntripMountpoint = "RTN4G_VRS_RTCM32";
    config.ntripUser = "";
    config.ntripPassword = "";
    config.ntripTimeout = 10000;

    // GGA defaults (Jaworzno, Poland)
    config.ggaLatitude = 50.2346;
    config.ggaLongitude = 19.2084;
    config.ggaAltitude = 266.0;
    config.ggaSendInterval = 2000;

    // LoRa defaults
    config.loraFrequency = 433000000; // 433 MHz
    config.loraSpreadingFactor = 7;
    config.loraBandwidth = 125000; // 125 kHz
    config.loraCodingRate = 5;
    config.loraTxPower = 20;
    config.loraSyncWord = 0x12;

    // RTCM message types (all types to transmit via LoRa)
    config.rtcmMessageTypes = {1005, 1007, 1019, 1020, 1033, 1075, 1085, 1095, 1125, 1230};

    // RTCM high-priority types (MSM observations + station position — critical for RTK)
    config.rtcmPriorityMessageTypes = {1005, 1075, 1085, 1095, 1125, 1230};

    // Display defaults
    config.displayUpdateInterval = 500;

    // Maintenance frame defaults
    config.maintenance.enabled = false;
    config.maintenance.intervalMs = 120000;
    config.maintenance.listenDurationMs = 2000;
    config.maintenance.rxWindowEnabled = false;
    config.maintenance.enabledCommands = {0x01, 0x05}; // RESET + PING enabled by default
    config.maintenance.deferredStopEnabled = false;
    config.maintenance.activeWindowMs = 300000; // 5 minutes
}

bool ConfigManager::loadConfig()
{
    if (!LittleFS.exists(CONFIG_FILE))
    {
#ifdef DEBUG
        Serial.println("ConfigManager: Config file not found, using defaults");
#endif
        return saveConfig(); // Create default config file
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file)
    {
#ifdef DEBUG
        Serial.println("ConfigManager: Failed to open config file");
#endif
        return false;
    }

    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error)
    {
#ifdef DEBUG
        Serial.print("ConfigManager: Failed to parse config: ");
        Serial.println(error.c_str());
#endif
        return false;
    }

    // Load WiFi config (use current defaults if not in JSON)
    config.wifiSSID = doc["wifi"]["ssid"] | config.wifiSSID;
    config.wifiPassword = doc["wifi"]["password"] | config.wifiPassword;
    config.wifiTimeout = doc["wifi"]["timeout"] | config.wifiTimeout;

    // Load NTRIP config
    config.ntripHost = doc["ntrip"]["host"] | config.ntripHost;
    config.ntripPort = doc["ntrip"]["port"] | config.ntripPort;
    config.ntripMountpoint = doc["ntrip"]["mountpoint"] | config.ntripMountpoint;
    config.ntripUser = doc["ntrip"]["user"] | config.ntripUser;
    config.ntripPassword = doc["ntrip"]["password"] | config.ntripPassword;
    config.ntripTimeout = doc["ntrip"]["timeout"] | config.ntripTimeout;

    // Load GGA config
    config.ggaLatitude = doc["gga"]["latitude"] | config.ggaLatitude;
    config.ggaLongitude = doc["gga"]["longitude"] | config.ggaLongitude;
    config.ggaAltitude = doc["gga"]["altitude"] | config.ggaAltitude;
    config.ggaSendInterval = doc["gga"]["sendInterval"] | config.ggaSendInterval;

    // Load LoRa config
    config.loraFrequency = doc["lora"]["frequency"] | config.loraFrequency;
    config.loraSpreadingFactor = doc["lora"]["spreadingFactor"] | config.loraSpreadingFactor;
    config.loraBandwidth = doc["lora"]["bandwidth"] | config.loraBandwidth;
    config.loraCodingRate = doc["lora"]["codingRate"] | config.loraCodingRate;
    config.loraTxPower = doc["lora"]["txPower"] | config.loraTxPower;
    config.loraSyncWord = doc["lora"]["syncWord"] | config.loraSyncWord;

    // Load RTCM message types (support both old and new config format)
    config.rtcmMessageTypes.clear();
    JsonArray msgTypes = doc["rtcm"]["messageTypes"];
    if (msgTypes.size() > 0)
    {
        for (JsonVariant v : msgTypes)
        {
            config.rtcmMessageTypes.push_back(v.as<uint16_t>());
        }
    }
    else
    {
        // Migration: merge old liveMessageTypes + bufferedMessageTypes
        JsonArray liveTypes = doc["rtcm"]["liveMessageTypes"];
        JsonArray bufferedTypes = doc["rtcm"]["bufferedMessageTypes"];
        if (liveTypes.size() > 0 || bufferedTypes.size() > 0)
        {
            for (JsonVariant v : liveTypes)
                config.rtcmMessageTypes.push_back(v.as<uint16_t>());
            for (JsonVariant v : bufferedTypes)
                config.rtcmMessageTypes.push_back(v.as<uint16_t>());
        }
        else
        {
            config.rtcmMessageTypes = {1005, 1007, 1019, 1020, 1033, 1075, 1085, 1095, 1125, 1230};
        }
    }

    // Load RTCM priority message types
    config.rtcmPriorityMessageTypes.clear();
    JsonArray priorityTypes = doc["rtcm"]["priorityMessageTypes"];
    if (priorityTypes.size() > 0)
    {
        for (JsonVariant v : priorityTypes)
        {
            config.rtcmPriorityMessageTypes.push_back(v.as<uint16_t>());
        }
    }
    else
    {
        config.rtcmPriorityMessageTypes = {1005, 1075, 1085, 1095, 1125, 1230};
    }

    // Load display config
    config.displayUpdateInterval = doc["timing"]["displayUpdate"] | doc["display"]["updateInterval"] | 500;

    // Load maintenance config
    config.maintenance.enabled = doc["maintenance"]["enabled"] | config.maintenance.enabled;
    config.maintenance.intervalMs = doc["maintenance"]["intervalMs"] | config.maintenance.intervalMs;
    config.maintenance.listenDurationMs = doc["maintenance"]["listenDurationMs"] | config.maintenance.listenDurationMs;
    config.maintenance.rxWindowEnabled = doc["maintenance"]["rxWindowEnabled"] | config.maintenance.rxWindowEnabled;
    config.maintenance.enabledCommands.clear();
    JsonArray enabledCmds = doc["maintenance"]["enabledCommands"];
    if (enabledCmds.size() > 0)
    {
        for (JsonVariant v : enabledCmds)
            config.maintenance.enabledCommands.push_back(v.as<uint8_t>());
    }
    else
    {
        config.maintenance.enabledCommands = {0x01, 0x05};
    }
    config.maintenance.deferredStopEnabled = doc["maintenance"]["deferredStopEnabled"] | config.maintenance.deferredStopEnabled;
    config.maintenance.activeWindowMs = doc["maintenance"]["activeWindowMs"] | config.maintenance.activeWindowMs;

#ifdef DEBUG
    Serial.println("ConfigManager: Configuration loaded successfully");
    printConfig();
#endif

    return true;
}

bool ConfigManager::saveConfig()
{
    DynamicJsonDocument doc(3072);

    // WiFi config
    doc["wifi"]["ssid"] = config.wifiSSID;
    doc["wifi"]["password"] = config.wifiPassword;
    doc["wifi"]["timeout"] = config.wifiTimeout;

    // NTRIP config
    doc["ntrip"]["host"] = config.ntripHost;
    doc["ntrip"]["port"] = config.ntripPort;
    doc["ntrip"]["mountpoint"] = config.ntripMountpoint;
    doc["ntrip"]["user"] = config.ntripUser;
    doc["ntrip"]["password"] = config.ntripPassword;
    doc["ntrip"]["timeout"] = config.ntripTimeout;

    // GGA config
    doc["gga"]["latitude"] = config.ggaLatitude;
    doc["gga"]["longitude"] = config.ggaLongitude;
    doc["gga"]["altitude"] = config.ggaAltitude;
    doc["gga"]["sendInterval"] = config.ggaSendInterval;

    // LoRa config
    doc["lora"]["frequency"] = config.loraFrequency;
    doc["lora"]["spreadingFactor"] = config.loraSpreadingFactor;
    doc["lora"]["bandwidth"] = config.loraBandwidth;
    doc["lora"]["codingRate"] = config.loraCodingRate;
    doc["lora"]["txPower"] = config.loraTxPower;
    doc["lora"]["syncWord"] = config.loraSyncWord;

    // RTCM message types
    JsonObject rtcmObj = doc.createNestedObject("rtcm");
    JsonArray msgTypes = rtcmObj.createNestedArray("messageTypes");
    for (uint16_t type : config.rtcmMessageTypes)
    {
        msgTypes.add(type);
    }

    // RTCM priority message types
    JsonArray priorityTypes = rtcmObj.createNestedArray("priorityMessageTypes");
    for (uint16_t type : config.rtcmPriorityMessageTypes)
    {
        priorityTypes.add(type);
    }

    // Display config
    doc["display"]["updateInterval"] = config.displayUpdateInterval;

    // Maintenance config
    JsonObject maintObj = doc.createNestedObject("maintenance");
    maintObj["enabled"] = config.maintenance.enabled;
    maintObj["intervalMs"] = config.maintenance.intervalMs;
    maintObj["listenDurationMs"] = config.maintenance.listenDurationMs;
    maintObj["rxWindowEnabled"] = config.maintenance.rxWindowEnabled;
    JsonArray maintCmds = maintObj.createNestedArray("enabledCommands");
    for (uint8_t cmd : config.maintenance.enabledCommands)
        maintCmds.add(cmd);
    maintObj["deferredStopEnabled"] = config.maintenance.deferredStopEnabled;
    maintObj["activeWindowMs"] = config.maintenance.activeWindowMs;

    File file = LittleFS.open(CONFIG_FILE, "w");
    if (!file)
    {
#ifdef DEBUG
        Serial.println("ConfigManager: Failed to create config file");
#endif
        return false;
    }

    if (serializeJson(doc, file) == 0)
    {
#ifdef DEBUG
        Serial.println("ConfigManager: Failed to write config file");
#endif
        file.close();
        return false;
    }

    file.close();

#ifdef DEBUG
    Serial.println("ConfigManager: Configuration saved successfully");
#endif

    return true;
}

SystemConfig &ConfigManager::getConfig()
{
    return config;
}

void ConfigManager::printConfig()
{
#ifdef DEBUG
    Serial.println("\n===== System Configuration =====");
    Serial.printf("WiFi SSID: %s\n", config.wifiSSID.c_str());
    Serial.printf("NTRIP Host: %s:%d\n", config.ntripHost.c_str(), config.ntripPort);
    Serial.printf("NTRIP Mountpoint: %s\n", config.ntripMountpoint.c_str());
    Serial.printf("GGA Position: %.4f, %.4f @ %.1fm\n", config.ggaLatitude, config.ggaLongitude, config.ggaAltitude);
    Serial.printf("LoRa Freq: %lu Hz, SF: %d, BW: %lu Hz\n", config.loraFrequency, config.loraSpreadingFactor, config.loraBandwidth);
    Serial.printf("RTCM Message Types: %d (%d priority)\n", config.rtcmMessageTypes.size(), config.rtcmPriorityMessageTypes.size());
    Serial.println("================================\n");
#endif
}
