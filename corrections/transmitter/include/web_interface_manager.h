#ifndef WEB_INTERFACE_MANAGER_H
#define WEB_INTERFACE_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "config_manager.h"
#include "ntrip_client.h"

class WebInterfaceManager
{
public:
    WebInterfaceManager(ConfigManager &configMgr, NTRIPClient &ntripCl);
    bool begin();
    void loop();
    bool isAPActive();
    bool hasClients();

private:
    ConfigManager &configManager;
    NTRIPClient &ntripClient;
    AsyncWebServer *server;

    bool apActive;
    unsigned long apStartTime;
    unsigned long lastClientDisconnectTime;
    uint8_t connectedClients;

    String apSSID;

    static const unsigned long AP_INITIAL_TIMEOUT = 180000; // 3 minutes on boot
    static const unsigned long AP_CLIENT_TIMEOUT = 180000;  // 3 minutes after last client disconnect

    void startAP();
    void stopAP();
    void checkAPTimeout();
    void updateClientCount();
    String generateAPName();

    // Web server handlers
    void setupRoutes();
    void handleRoot(AsyncWebServerRequest *request);
    void handleGetConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    void handleRestart(AsyncWebServerRequest *request);
    void handleWiFiScan(AsyncWebServerRequest *request);
    void handleGetMountpoints(AsyncWebServerRequest *request);
};

#endif
