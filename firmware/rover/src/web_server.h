#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "settings.h"
#include "transport.h"
#include "wifi_manager.h"

/**
 * @brief Config web UI (static files from LittleFS) and its REST API.
 *
 *   GET  /api/status     live state; ?ka=1 (sent by an open, in-use page) extends the setup window
 *   GET  /api/config     current settings (passwords are never returned)
 *   POST /api/config     validate + save + restart
 *   POST /api/restart    restart without saving
 *   POST /api/wifi/off   end the setup window now: WiFi off, Bluetooth starts
 *   GET  /api/wifi/scan  async scan, poll until "scanning" is false
 *
 * No authentication yet - anyone who can reach the device can change it.
 */
class RoverWebServer
{
public:
    void begin(RoverSettings *settings, SettingsStore *store, WifiManager *wifi, ITransport *const *transport);
    void end();

private:
    void handleStatus(AsyncWebServerRequest *request);
    void handleGetConfig(AsyncWebServerRequest *request);
    void handleSaveConfig(AsyncWebServerRequest *request, JsonVariant &json);
    void handleScan(AsyncWebServerRequest *request);
    void sendError(AsyncWebServerRequest *request, int code, const char *message);

    AsyncWebServer *_server = nullptr;
    RoverSettings *_settings = nullptr;
    SettingsStore *_store = nullptr;
    WifiManager *_wifi = nullptr;
    ITransport *const *_transport = nullptr;
};

#endif // WEB_SERVER_H
