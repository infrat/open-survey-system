#ifndef TELNET_SERVICE_H
#define TELNET_SERVICE_H

#include <Arduino.h>
#include <WiFi.h>
#include "transport.h"
#include "wifi_manager.h"

/**
 * @brief Telnet debug console: mirrors the log and answers a few commands.
 *        A connected client keeps the WiFi window open.
 */
class TelnetService
{
public:
    TelnetService() : _server(TELNET_PORT) {}

    void begin(WifiManager *wifi, ITransport *const *transport);
    void loop();
    void stop();

private:
    void handleCommands();

    WiFiServer _server;
    WiFiClient _client;
    WifiManager *_wifi = nullptr;
    ITransport *const *_transport = nullptr;
    bool _running = false;
};

#endif // TELNET_SERVICE_H
