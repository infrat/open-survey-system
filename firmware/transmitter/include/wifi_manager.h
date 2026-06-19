#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "config_manager.h"

class WiFiManager
{
public:
    WiFiManager(ConfigManager &configMgr);
    bool connect();
    bool isConnected();

private:
    ConfigManager &configManager;
    unsigned long lastConnectionAttempt;
    static const unsigned long CONNECTION_RETRY_INTERVAL = 30000; // 30 seconds
};

#endif