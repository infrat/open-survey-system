#ifndef NTRIP_CLIENT_H
#define NTRIP_CLIENT_H

#include <WiFiClient.h>
#include <Arduino.h>
#include "config_manager.h"

class NTRIPClient
{
public:
    NTRIPClient(ConfigManager &configMgr);
    bool connect();
    bool isConnected();
    void disconnect();
    size_t available();
    size_t read(uint8_t *buffer, size_t length);
    void keepAlive();
    bool testConnectivity();                                                                              // Test basic connectivity to NTRIP host
    String getMountpoints(const String &host, uint16_t port, const String &user, const String &password); // Get available mountpoints

private:
    ConfigManager &configManager;
    WiFiClient client;
    unsigned long lastDataTime;
    unsigned long lastGGATime;
    unsigned long lastConnectionAttempt;
    bool connected;

    String generateGGAString();
    String encodeBase64(const String &input);
    void sendGGA();

    static const unsigned long DATA_TIMEOUT = 60000;              // 60 seconds
    static const unsigned long CONNECTION_RETRY_INTERVAL = 30000; // 30 seconds
};

#endif