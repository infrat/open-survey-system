#include "telnet_service.h"
#include "logger.h"
#include "system_control.h"

void TelnetService::begin(WifiManager *wifi, ITransport *const *transport)
{
    if (_running)
    {
        return;
    }
    _wifi = wifi;
    _transport = transport;

    _server.begin();
    _server.setNoDelay(true);
    _running = true;

    logPrintln("[Telnet] Server started on port %d", TELNET_PORT);
    logPrint("[Telnet] Connect with: telnet %s.local\n", OTA_HOSTNAME);
}

void TelnetService::stop()
{
    if (!_running)
    {
        return;
    }
    logSetMirror(nullptr);
    if (_client)
    {
        _client.stop();
    }
    _server.stop();
    _running = false;
}

void TelnetService::loop()
{
    if (!_running)
    {
        return;
    }

    if (_server.hasClient())
    {
        // Only one console at a time, the newest wins
        if (_client && _client.connected())
        {
            _client.stop();
        }

        _client = _server.available();
        logSetMirror(&_client);
        logPrintln("[Telnet] Client connected from %s", _client.remoteIP().toString().c_str());
        _client.println("=== OSS RTK Rover - ESP32 Debug Console ===");
        _client.println("Type 'help' for available commands\n");
    }

    if (_client && !_client.connected())
    {
        logSetMirror(nullptr);
        _client.stop();
    }

    if (_client && _client.connected())
    {
        _wifi->keepAlive();
        handleCommands();
    }
}

void TelnetService::handleCommands()
{
    while (_client.available())
    {
        String cmd = _client.readStringUntil('\n');
        cmd.trim();

        if (cmd == "status")
        {
            ITransport *t = _transport ? *_transport : nullptr;
            if (t)
            {
                logPrintln("[Status] %s: %s (%u client(s))", t->name(), t->isConnected() ? "Connected" : "Disconnected", (unsigned)t->getConnectedCount());
            }
            logPrintln("[Status] WiFi: %s", _wifi->enabled() ? "Enabled" : "Disabled");
            logPrintln("[Status] Free Heap: %d bytes", ESP.getFreeHeap());
            logPrintln("[Status] Uptime: %lu seconds", millis() / 1000);
        }
        else if (cmd == "restart")
        {
            logPrintln("[Telnet] Restarting ESP32...");
            scheduleRestart(500);
        }
        else if (cmd == "help")
        {
            logPrintln("Available commands:");
            logPrintln("  status  - Show system status");
            logPrintln("  restart - Restart ESP32");
            logPrintln("  help    - Show this help");
        }
    }
}
