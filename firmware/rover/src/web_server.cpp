#include "web_server.h"
#include <AsyncJson.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "config.h"
#include "logger.h"
#include "system_control.h"

void RoverWebServer::begin(RoverSettings *settings, SettingsStore *store, WifiManager *wifi, ITransport *const *transport)
{
    _settings = settings;
    _store = store;
    _wifi = wifi;
    _transport = transport;

    // true = format on first use, so a fresh partition never blocks the boot
    if (!LittleFS.begin(true))
    {
        logPrintln("[Web] LittleFS mount failed - UI files unavailable");
    }

    _server = new AsyncWebServer(HTTP_PORT);

    _server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request)
                { handleStatus(request); });

    _server->on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
                {
                    _wifi->keepAlive();
                    handleGetConfig(request); });

    AsyncCallbackJsonWebHandler *saveHandler = new AsyncCallbackJsonWebHandler(
        "/api/config", [this](AsyncWebServerRequest *request, JsonVariant &json)
        {
            _wifi->keepAlive();
            handleSaveConfig(request, json); });
    saveHandler->setMethod(HTTP_POST);
    saveHandler->setMaxContentLength(1024);
    _server->addHandler(saveHandler);

    _server->on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request)
                {
                    _wifi->keepAlive();
                    request->send(200, "application/json", "{\"success\":true,\"message\":\"Device restarting...\"}");
                    scheduleRestart(1000); });

    _server->on("/api/wifi/off", HTTP_POST, [this](AsyncWebServerRequest *request)
                {
                    if (_wifi->alwaysOn())
                    {
                        sendError(request, 400, "WiFi stays on with the WiFi (TCP) link");
                        return;
                    }
                    request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi off, starting Bluetooth\"}");
                    _wifi->finishSoon(500); });

    _server->on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
                {
                    _wifi->keepAlive();
                    handleScan(request); });

    _server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                {
                    if (LittleFS.exists("/index.html"))
                    {
                        request->send(LittleFS, "/index.html", "text/html");
                    }
                    else
                    {
                        request->send(200, "text/html",
                                      "<html><body><h1>OSS RTK Rover</h1>"
                                      "<p>Web UI files not found. Upload the filesystem image "
                                      "(pio run -t uploadfs).</p></body></html>");
                    } });

    _server->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    _server->onNotFound([](AsyncWebServerRequest *request)
                        { request->send(404, "text/plain", "Not found"); });

    _server->begin();
    logPrintln("[Web] UI on port %d", HTTP_PORT);
}

void RoverWebServer::end()
{
    if (_server)
    {
        _server->end();
    }
}

void RoverWebServer::sendError(AsyncWebServerRequest *request, int code, const char *message)
{
    StaticJsonDocument<192> doc;
    doc["success"] = false;
    doc["message"] = message;
    String out;
    serializeJson(doc, out);
    request->send(code, "application/json", out);
}

void RoverWebServer::handleStatus(AsyncWebServerRequest *request)
{
    // Only the page's explicit heartbeat extends the setup window. It stops when
    // the tab is hidden or unused, so a forgotten tab cannot keep Bluetooth off.
    if (request->hasParam("ka"))
    {
        _wifi->keepAlive();
    }

    StaticJsonDocument<768> doc;
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["maxAlloc"] = ESP.getMaxAllocHeap(); // largest free block: what a response buffer can get
    doc["minFreeHeap"] = ESP.getMinFreeHeap();

    ITransport *t = _transport ? *_transport : nullptr;
    JsonObject tr = doc.createNestedObject("transport");
    tr["mode"] = transportModeId(_settings->transport);
    tr["name"] = t ? t->name() : "-";
    tr["active"] = t != nullptr; // false during the WiFi-only setup window
    tr["connected"] = t ? t->isConnected() : false;
    tr["clients"] = t ? t->getConnectedCount() : 0;

    JsonObject w = doc.createNestedObject("wifi");
    w["mode"] = wifiModeId(_wifi->mode());
    w["enabled"] = _wifi->enabled();
    w["remainingSec"] = _wifi->remainingSec();
    w["staConnected"] = _wifi->staConnected();
    w["staSsid"] = _wifi->staConnected() ? WiFi.SSID() : String("");
    w["staIp"] = _wifi->staIp();
    w["rssi"] = _wifi->staConnected() ? WiFi.RSSI() : 0;
    w["apRunning"] = _wifi->apRunning();
    w["apSsid"] = _wifi->apSsid();
    w["apIp"] = _wifi->apIp();
    w["apClients"] = _wifi->apClients();

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void RoverWebServer::handleGetConfig(AsyncWebServerRequest *request)
{
    StaticJsonDocument<512> doc;
    doc["transport"] = transportModeId(_settings->transport);
    doc["tcpPort"] = _settings->tcpPort;

    JsonObject w = doc.createNestedObject("wifi");
    w["mode"] = wifiModeId(_settings->wifiMode);
    w["staSsid"] = _settings->staSsid;
    w["staPasswordSet"] = _settings->staPassword.length() > 0;
    w["apSsid"] = _settings->apSsid;
    w["apPasswordSet"] = _settings->apPassword.length() > 0;
    w["windowSec"] = _settings->setupWindowSec;

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}

void RoverWebServer::handleSaveConfig(AsyncWebServerRequest *request, JsonVariant &json)
{
    JsonObject root = json.as<JsonObject>();
    if (root.isNull())
    {
        sendError(request, 400, "Invalid JSON");
        return;
    }

    // Start from the current values: missing keys stay unchanged
    RoverSettings next = *_settings;

    if (root.containsKey("transport"))
    {
        String id = root["transport"].as<String>();
        if (id == "ble")
            next.transport = TransportMode::BLE;
        else if (id == "spp")
            next.transport = TransportMode::SPP;
        else if (id == "tcp")
            next.transport = TransportMode::TCP;
        else
        {
            sendError(request, 400, "Unknown transport");
            return;
        }
    }
    if (root.containsKey("tcpPort"))
    {
        uint32_t port = root["tcpPort"].as<uint32_t>();
        if (port < 1 || port > 65535)
        {
            sendError(request, 400, "TCP port must be 1-65535");
            return;
        }
        next.tcpPort = (uint16_t)port;
    }

    JsonObject w = root["wifi"];
    if (!w.isNull())
    {
        if (w.containsKey("mode"))
        {
            String id = w["mode"].as<String>();
            if (id == "ap")
                next.wifiMode = WifiMode::AP;
            else if (id == "sta")
                next.wifiMode = WifiMode::STA;
            else if (id == "ap_sta")
                next.wifiMode = WifiMode::AP_STA;
            else
            {
                sendError(request, 400, "Unknown WiFi mode");
                return;
            }
        }
        if (w.containsKey("staSsid"))
            next.staSsid = w["staSsid"].as<String>();
        // Passwords are write-only: absent = keep, present (even "") = replace
        if (w.containsKey("staPassword"))
            next.staPassword = w["staPassword"].as<String>();
        if (w.containsKey("apSsid"))
            next.apSsid = w["apSsid"].as<String>();
        if (w.containsKey("apPassword"))
            next.apPassword = w["apPassword"].as<String>();
        if (w.containsKey("windowSec"))
        {
            uint32_t seconds = w["windowSec"].as<uint32_t>();
            if (seconds < SETUP_WINDOW_MIN_SEC || seconds > SETUP_WINDOW_MAX_SEC)
            {
                sendError(request, 400, "Setup window must be 15-600 seconds");
                return;
            }
            next.setupWindowSec = (uint16_t)seconds;
        }
    }

    String error;
    if (!next.validate(error))
    {
        sendError(request, 400, error.c_str());
        return;
    }

    if (!_store->save(next))
    {
        sendError(request, 500, "Failed to save settings");
        return;
    }

    *_settings = next;
    logPrintln("[Web] Settings saved - transport=%s wifi=%s, restarting", transportModeId(next.transport), wifiModeId(next.wifiMode));
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Settings saved. Device will restart.\"}");
    scheduleRestart(1500);
}

void RoverWebServer::handleScan(AsyncWebServerRequest *request)
{
    // The STA interface has to exist for a scan
    if (WiFi.getMode() == WIFI_MODE_AP)
    {
        sendError(request, 400, "Scanning needs Station or AP+Station mode");
        return;
    }

    int16_t state = WiFi.scanComplete();
    if (state == WIFI_SCAN_FAILED)
    {
        WiFi.scanNetworks(true); // async, the UI polls this endpoint
        request->send(200, "application/json", "{\"success\":true,\"scanning\":true,\"networks\":[]}");
        return;
    }
    if (state == WIFI_SCAN_RUNNING)
    {
        request->send(200, "application/json", "{\"success\":true,\"scanning\":true,\"networks\":[]}");
        return;
    }

    DynamicJsonDocument doc(2048);
    doc["success"] = true;
    doc["scanning"] = false;
    JsonArray networks = doc.createNestedArray("networks");
    for (int i = 0; i < state && i < 15; i++)
    {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
        net["secured"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
        net["channel"] = WiFi.channel(i);
    }
    WiFi.scanDelete();

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
}
