#include "ota_service.h"
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include "config.h"
#include "logger.h"

static WifiManager *g_wifi = nullptr;

void OtaService::begin(WifiManager *wifi)
{
    if (_running)
    {
        return;
    }
    g_wifi = wifi;

    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]()
                       {
        if (g_wifi) g_wifi->keepAlive();
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        // The web UI partition is about to be rewritten, stop serving from it
        if (ArduinoOTA.getCommand() != U_FLASH) LittleFS.end();
        logPrintln("[OTA] Update started: %s", type.c_str()); });

    ArduinoOTA.onEnd([]()
                     { logPrintln("\n[OTA] Update complete!"); });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                          {
        if (g_wifi) g_wifi->keepAlive();
        logPrint("[OTA] Progress: %u%%\r", (progress / (total / 100))); });

    ArduinoOTA.onError([](ota_error_t error)
                       {
        logPrint("[OTA] Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) logPrintln("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) logPrintln("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) logPrintln("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) logPrintln("Receive Failed");
        else if (error == OTA_END_ERROR) logPrintln("End Failed"); });

    ArduinoOTA.begin();
    MDNS.addService("http", "tcp", HTTP_PORT);
    _running = true;

    logPrintln("[OTA] Ready for firmware updates");
    logPrint("[OTA] Hostname: %s.local\n", OTA_HOSTNAME);
}

void OtaService::loop()
{
    if (_running)
    {
        ArduinoOTA.handle();
    }
}

void OtaService::end()
{
    if (_running)
    {
        ArduinoOTA.end();
        _running = false;
    }
}
