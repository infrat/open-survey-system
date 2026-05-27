#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <WiFiAP.h>

// WiFi functions
void setupWiFiHotspot();
void handleWiFiClients();
bool isWiFiConnected();
String getWiFiStatus();
int getConnectedClients();

#endif // WIFI_MANAGER_H