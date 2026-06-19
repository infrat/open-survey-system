#include "wifi_manager.h"
#include "config.h"
#include "debug.h"

void setupWiFiHotspot()
{
    DEBUG_PRINTLN("Setting up WiFi Hotspot...");

    // Configure WiFi as Access Point
    WiFi.mode(WIFI_AP);

    // Start the Access Point
    bool result = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD, WIFI_CHANNEL, 0, WIFI_MAX_CONNECTIONS);

    if (result)
    {
        IPAddress IP = WiFi.softAPIP();
        DEBUG_PRINTF("WiFi Hotspot started successfully\n");
        DEBUG_PRINTF("SSID: %s\n", WIFI_SSID);
        DEBUG_PRINTF("Password: %s\n", WIFI_PASSWORD);
        DEBUG_PRINTF("IP Address: %s\n", IP.toString().c_str());
        DEBUG_PRINTF("Channel: %d\n", WIFI_CHANNEL);
        DEBUG_PRINTF("Max Connections: %d\n", WIFI_MAX_CONNECTIONS);
    }
    else
    {
        DEBUG_PRINTLN("Failed to start WiFi Hotspot!");
    }
}

void handleWiFiClients()
{
    // WiFi client handling is automatic in ESP32
    // This function can be used for additional client management if needed
}

bool isWiFiConnected()
{
    return WiFi.getMode() == WIFI_AP && WiFi.softAPgetStationNum() > 0;
}

String getWiFiStatus()
{
    if (WiFi.getMode() == WIFI_AP)
    {
        return "AP: " + String(WiFi.softAPgetStationNum()) + "/" + String(WIFI_MAX_CONNECTIONS);
    }
    return "OFF";
}

int getConnectedClients()
{
    return WiFi.softAPgetStationNum();
}