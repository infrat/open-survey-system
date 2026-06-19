#include "web_interface_manager.h"
#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_task_wdt.h>

WebInterfaceManager::WebInterfaceManager(ConfigManager &configMgr, NTRIPClient &ntripCl)
    : configManager(configMgr), ntripClient(ntripCl), server(nullptr), apActive(false),
      apStartTime(0), lastClientDisconnectTime(0), connectedClients(0)
{
}

bool WebInterfaceManager::begin()
{
    apSSID = generateAPName();

    server = new AsyncWebServer(80);
    setupRoutes();

    startAP();

    return true;
}

void WebInterfaceManager::loop()
{
    if (apActive)
    {
        updateClientCount();
        checkAPTimeout();
    }
}

bool WebInterfaceManager::isAPActive()
{
    return apActive;
}

bool WebInterfaceManager::hasClients()
{
    return connectedClients > 0;
}

String WebInterfaceManager::generateAPName()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA); // Read MAC directly without WiFi.begin()
    char apName[32];
    snprintf(apName, sizeof(apName), "OSS-LoRa-TX-%02X%02X", mac[4], mac[5]);
    return String(apName);
}

void WebInterfaceManager::startAP()
{
    if (apActive)
        return;

    // Generate AP name if not already set
    if (apSSID.length() == 0)
    {
        apSSID = generateAPName();
    }

#ifdef DEBUG
    Serial.println("WebInterface: Starting AP mode");
    Serial.printf("AP SSID: %s (no password)\n", apSSID.c_str());
#endif

    // Start only AP mode (STA will be enabled by WiFiManager later)
    WiFi.mode(WIFI_MODE_APSTA);
    delay(100);
    WiFi.softAP(apSSID.c_str());

    IPAddress IP = WiFi.softAPIP();
#ifdef DEBUG
    Serial.print("AP IP address: ");
    Serial.println(IP);
#endif

    server->begin();

    apActive = true;
    apStartTime = millis();
    connectedClients = 0;

#ifdef DEBUG
    Serial.println("WebInterface: AP started successfully");
#endif
}

void WebInterfaceManager::stopAP()
{
    if (!apActive)
        return;

#ifdef DEBUG
    Serial.println("WebInterface: Stopping AP mode");
#endif

    server->end();
    WiFi.softAPdisconnect(true);

    apActive = false;

#ifdef DEBUG
    Serial.println("WebInterface: AP stopped");
#endif
}

void WebInterfaceManager::checkAPTimeout()
{
    unsigned long currentTime = millis();

    // If we have clients, don't timeout
    if (connectedClients > 0)
    {
        lastClientDisconnectTime = 0; // Reset disconnect timer
        return;
    }

    // If no clients and we just started, check initial timeout
    if (lastClientDisconnectTime == 0)
    {
        if (currentTime - apStartTime >= AP_INITIAL_TIMEOUT)
        {
#ifdef DEBUG
            Serial.println("WebInterface: Initial AP timeout reached, stopping AP");
#endif
            stopAP();
        }
    }
    // If clients disconnected, check client timeout
    else
    {
        if (currentTime - lastClientDisconnectTime >= AP_CLIENT_TIMEOUT)
        {
#ifdef DEBUG
            Serial.println("WebInterface: Client disconnect timeout reached, stopping AP");
#endif
            stopAP();
        }
    }
}

void WebInterfaceManager::updateClientCount()
{
    uint8_t newClientCount = WiFi.softAPgetStationNum();

    if (newClientCount != connectedClients)
    {
#ifdef DEBUG
        Serial.printf("WebInterface: Client count changed: %d -> %d\n", connectedClients, newClientCount);
#endif

        if (newClientCount == 0 && connectedClients > 0)
        {
            // Clients just disconnected
            lastClientDisconnectTime = millis();
#ifdef DEBUG
            Serial.println("WebInterface: All clients disconnected, starting timeout");
#endif
        }
        else if (newClientCount > 0 && connectedClients == 0)
        {
            // First client connected
            lastClientDisconnectTime = 0;
#ifdef DEBUG
            Serial.println("WebInterface: Client connected");
#endif
        }

        connectedClients = newClientCount;
    }
}

void WebInterfaceManager::setupRoutes()
{
    // Serve index.html
    server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleRoot(request); });

    // API: Get current configuration
    server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
               { handleGetConfig(request); });

    // API: Save configuration
    server->on("/api/config", HTTP_POST, [this](AsyncWebServerRequest *request)
               {
                   // This will be called after body handler
               },
               NULL, [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
               { handleSaveConfig(request, data, len, index, total); });

    // API: Restart device
    server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleRestart(request); });

    // API: WiFi scan (synchronous)
    server->on("/api/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleWiFiScan(request); });

    // API: Get NTRIP mountpoints
    server->on("/api/ntrip/mountpoints", HTTP_POST, [this](AsyncWebServerRequest *request)
               { handleGetMountpoints(request); });

    // Serve static files from LittleFS
    server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 404 handler
    server->onNotFound([](AsyncWebServerRequest *request)
                       { request->send(404, "text/plain", "Not found"); });
}

void WebInterfaceManager::handleRoot(AsyncWebServerRequest *request)
{
    if (LittleFS.exists("/index.html"))
    {
        request->send(LittleFS, "/index.html", "text/html");
    }
    else
    {
        request->send(200, "text/html",
                      "<html><body><h1>RTCM Transmitter</h1>"
                      "<p>Web interface files not found. Please upload filesystem image.</p>"
                      "</body></html>");
    }
}

void WebInterfaceManager::handleGetConfig(AsyncWebServerRequest *request)
{
    DynamicJsonDocument doc(3072);
    SystemConfig &cfg = configManager.getConfig();

    // WiFi
    doc["wifi"]["ssid"] = cfg.wifiSSID;
    doc["wifi"]["password"] = cfg.wifiPassword;
    doc["wifi"]["timeout"] = cfg.wifiTimeout;

    // NTRIP
    doc["ntrip"]["host"] = cfg.ntripHost;
    doc["ntrip"]["port"] = cfg.ntripPort;
    doc["ntrip"]["mountpoint"] = cfg.ntripMountpoint;
    doc["ntrip"]["user"] = cfg.ntripUser;
    doc["ntrip"]["password"] = cfg.ntripPassword;
    doc["ntrip"]["timeout"] = cfg.ntripTimeout;

    // GGA
    doc["gga"]["latitude"] = cfg.ggaLatitude;
    doc["gga"]["longitude"] = cfg.ggaLongitude;
    doc["gga"]["altitude"] = cfg.ggaAltitude;
    doc["gga"]["sendInterval"] = cfg.ggaSendInterval;

    // LoRa
    doc["lora"]["frequency"] = cfg.loraFrequency;
    doc["lora"]["spreadingFactor"] = cfg.loraSpreadingFactor;
    doc["lora"]["bandwidth"] = cfg.loraBandwidth;
    doc["lora"]["codingRate"] = cfg.loraCodingRate;
    doc["lora"]["txPower"] = cfg.loraTxPower;
    doc["lora"]["syncWord"] = cfg.loraSyncWord;

    // RTCM
    JsonObject rtcmObj = doc.createNestedObject("rtcm");
    JsonArray msgTypes = rtcmObj.createNestedArray("messageTypes");
    for (uint16_t type : cfg.rtcmMessageTypes)
    {
        msgTypes.add(type);
    }

    JsonArray priorityTypes = rtcmObj.createNestedArray("priorityMessageTypes");
    for (uint16_t type : cfg.rtcmPriorityMessageTypes)
    {
        priorityTypes.add(type);
    }

    // Display
    doc["display"]["updateInterval"] = cfg.displayUpdateInterval;

    // Maintenance
    JsonObject maintObj = doc.createNestedObject("maintenance");
    maintObj["enabled"] = cfg.maintenance.enabled;
    maintObj["intervalMs"] = cfg.maintenance.intervalMs;
    maintObj["listenDurationMs"] = cfg.maintenance.listenDurationMs;
    maintObj["rxWindowEnabled"] = cfg.maintenance.rxWindowEnabled;
    JsonArray maintCmds = maintObj.createNestedArray("enabledCommands");
    for (uint8_t cmd : cfg.maintenance.enabledCommands)
        maintCmds.add(cmd);
    maintObj["deferredStopEnabled"] = cfg.maintenance.deferredStopEnabled;
    maintObj["activeWindowMs"] = cfg.maintenance.activeWindowMs;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void WebInterfaceManager::handleSaveConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
    // Static buffer to accumulate body data
    static String bodyData;

    // First chunk - initialize
    if (index == 0)
    {
        bodyData = "";
    }

    // Accumulate data
    for (size_t i = 0; i < len; i++)
    {
        bodyData += (char)data[i];
    }

    // Not complete yet
    if (index + len < total)
    {
        return;
    }

    // Now we have complete body
    if (bodyData.length() == 0)
    {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"No data received\"}");
        return;
    }

    DynamicJsonDocument doc(3072);
    DeserializationError error = deserializeJson(doc, bodyData);

    if (error)
    {
#ifdef DEBUG
        Serial.print("WebInterface: JSON parse error: ");
        Serial.println(error.c_str());
#endif
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Invalid JSON\"}");
        return;
    }

    SystemConfig &cfg = configManager.getConfig();

    // Update WiFi config
    if (doc.containsKey("wifi"))
    {
        cfg.wifiSSID = doc["wifi"]["ssid"] | cfg.wifiSSID;
        cfg.wifiPassword = doc["wifi"]["password"] | cfg.wifiPassword;
        cfg.wifiTimeout = doc["wifi"]["timeout"] | cfg.wifiTimeout;
    }

    // Update NTRIP config
    if (doc.containsKey("ntrip"))
    {
        cfg.ntripHost = doc["ntrip"]["host"] | cfg.ntripHost;
        cfg.ntripPort = doc["ntrip"]["port"] | cfg.ntripPort;
        cfg.ntripMountpoint = doc["ntrip"]["mountpoint"] | cfg.ntripMountpoint;
        cfg.ntripUser = doc["ntrip"]["user"] | cfg.ntripUser;
        cfg.ntripPassword = doc["ntrip"]["password"] | cfg.ntripPassword;
        cfg.ntripTimeout = doc["ntrip"]["timeout"] | cfg.ntripTimeout;
    }

    // Update GGA config
    if (doc.containsKey("gga"))
    {
        cfg.ggaLatitude = doc["gga"]["latitude"] | cfg.ggaLatitude;
        cfg.ggaLongitude = doc["gga"]["longitude"] | cfg.ggaLongitude;
        cfg.ggaAltitude = doc["gga"]["altitude"] | cfg.ggaAltitude;
        cfg.ggaSendInterval = doc["gga"]["sendInterval"] | cfg.ggaSendInterval;
    }

    // Update LoRa config
    if (doc.containsKey("lora"))
    {
        cfg.loraFrequency = doc["lora"]["frequency"] | cfg.loraFrequency;
        cfg.loraSpreadingFactor = doc["lora"]["spreadingFactor"] | cfg.loraSpreadingFactor;
        cfg.loraBandwidth = doc["lora"]["bandwidth"] | cfg.loraBandwidth;
        cfg.loraCodingRate = doc["lora"]["codingRate"] | cfg.loraCodingRate;
        cfg.loraTxPower = doc["lora"]["txPower"] | cfg.loraTxPower;
        cfg.loraSyncWord = doc["lora"]["syncWord"] | cfg.loraSyncWord;
    }

    // Update RTCM config
    if (doc.containsKey("rtcm"))
    {
        if (doc["rtcm"].containsKey("messageTypes"))
        {
            cfg.rtcmMessageTypes.clear();
            JsonArray msgTypes = doc["rtcm"]["messageTypes"];
            for (JsonVariant v : msgTypes)
            {
                cfg.rtcmMessageTypes.push_back(v.as<uint16_t>());
            }
        }

        if (doc["rtcm"].containsKey("priorityMessageTypes"))
        {
            cfg.rtcmPriorityMessageTypes.clear();
            JsonArray priorityTypes = doc["rtcm"]["priorityMessageTypes"];
            for (JsonVariant v : priorityTypes)
            {
                cfg.rtcmPriorityMessageTypes.push_back(v.as<uint16_t>());
            }
        }
    }

    // Update display config
    if (doc.containsKey("display"))
    {
        cfg.displayUpdateInterval = doc["display"]["updateInterval"] | cfg.displayUpdateInterval;
    }

    // Update maintenance config
    if (doc.containsKey("maintenance"))
    {
        cfg.maintenance.enabled = doc["maintenance"]["enabled"] | cfg.maintenance.enabled;
        cfg.maintenance.intervalMs = doc["maintenance"]["intervalMs"] | cfg.maintenance.intervalMs;
        cfg.maintenance.listenDurationMs = doc["maintenance"]["listenDurationMs"] | cfg.maintenance.listenDurationMs;
        cfg.maintenance.rxWindowEnabled = doc["maintenance"]["rxWindowEnabled"] | cfg.maintenance.rxWindowEnabled;
        if (doc["maintenance"].containsKey("enabledCommands"))
        {
            cfg.maintenance.enabledCommands.clear();
            JsonArray cmds = doc["maintenance"]["enabledCommands"];
            for (JsonVariant v : cmds)
                cfg.maintenance.enabledCommands.push_back(v.as<uint8_t>());
        }
        cfg.maintenance.deferredStopEnabled = doc["maintenance"]["deferredStopEnabled"] | cfg.maintenance.deferredStopEnabled;
        cfg.maintenance.activeWindowMs = doc["maintenance"]["activeWindowMs"] | cfg.maintenance.activeWindowMs;
    }

    // Save to file
    if (configManager.saveConfig())
    {
#ifdef DEBUG
        Serial.println("WebInterface: Configuration saved successfully");
#endif
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Configuration saved. Device will restart.\"}");

        // Schedule restart
        delay(1000);
        ESP.restart();
    }
    else
    {
#ifdef DEBUG
        Serial.println("WebInterface: Failed to save configuration");
#endif
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Failed to save configuration\"}");
    }
}

void WebInterfaceManager::handleRestart(AsyncWebServerRequest *request)
{
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Device restarting...\"}");
    delay(1000);
    ESP.restart();
}

void WebInterfaceManager::handleWiFiScan(AsyncWebServerRequest *request)
{
#ifdef DEBUG
    Serial.println(F("WebInterface: Starting WiFi scan..."));
#endif

    // Temporarily increase watchdog timeout - scan may take 2-5 seconds
    esp_task_wdt_init(30, false); // 30s timeout, no panic

    // Simple synchronous scan
    int16_t scanResult = WiFi.scanNetworks(false, false); // false=sync, false=no hidden networks

    // Restore normal watchdog
    esp_task_wdt_init(5, true); // 5s timeout back, with panic

    if (scanResult == WIFI_SCAN_FAILED || scanResult < 0)
    {
#ifdef DEBUG
        Serial.println(F("WebInterface: WiFi scan failed"));
#endif
        request->send(500, "application/json",
                      "{\"success\":false,\"message\":\"Scan failed\"}");
        return;
    }

    if (scanResult == 0)
    {
#ifdef DEBUG
        Serial.println(F("WebInterface: No networks found"));
#endif
        request->send(200, "application/json",
                      "{\"success\":true,\"networks\":[]}");
        WiFi.scanDelete();
        return;
    }

#ifdef DEBUG
    Serial.printf("WebInterface: Found %d networks\n", scanResult);
#endif

    // Build JSON with network list
    StaticJsonDocument<2048> doc;
    doc["success"] = true;
    JsonArray networks = doc.createNestedArray("networks");

    for (int i = 0; i < scanResult; i++)
    {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["bssid"] = WiFi.BSSIDstr(i);
        net["rssi"] = WiFi.RSSI(i);
        net["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "secured";
        net["channel"] = WiFi.channel(i);
    }

    String response;
    serializeJson(doc, response);

    // Delete scan results
    WiFi.scanDelete();

    request->send(200, "application/json", response);

#ifdef DEBUG
    Serial.println(F("WebInterface: WiFi scan results sent"));
#endif
}

void WebInterfaceManager::handleGetMountpoints(AsyncWebServerRequest *request)
{
#ifdef DEBUG
    Serial.println(F("WebInterface: Getting NTRIP mountpoints..."));
#endif

    // Get parameters from POST body
    if (!request->hasParam("host", true) || !request->hasParam("port", true))
    {
        request->send(400, "application/json",
                      "{\"error\":\"Missing host or port parameter\"}");
        return;
    }

    String host = request->getParam("host", true)->value();
    uint16_t port = request->getParam("port", true)->value().toInt();
    String user = request->hasParam("user", true) ? request->getParam("user", true)->value() : "";
    String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";

#ifdef DEBUG
    Serial.printf("Fetching mountpoints for %s:%d\n", host.c_str(), port);
#endif

    // Call NTRIPClient method to get mountpoints
    String result = ntripClient.getMountpoints(host, port, user, password);

#ifdef DEBUG
    Serial.println("Mountpoints result:");
    Serial.println(result);
#endif

    request->send(200, "application/json", result);
}
