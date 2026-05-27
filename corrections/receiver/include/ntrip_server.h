#ifndef NTRIP_SERVER_H
#define NTRIP_SERVER_H

#include <WiFi.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "config.h"

// NTRIP Server functions
void setupNTRIPServer();
void handleNTRIPClients();
void broadcastRTCMData(const uint8_t *data, size_t length);
void handleNewNTRIPClient(WiFiClient &client);
void removeDisconnectedClients();
int getNTRIPClientCount();
void sendSourceTable(WiFiClient &client);

// NTRIP Client structure
struct NTRIPClient
{
    WiFiClient client;
    bool authenticated;
    unsigned long lastActivity;
    bool active;
};

extern NTRIPClient ntripClients[NTRIP_MAX_CLIENTS];
extern int activeClientCount;

#endif // NTRIP_SERVER_H