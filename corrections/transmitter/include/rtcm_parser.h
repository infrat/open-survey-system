#ifndef RTCM_PARSER_H
#define RTCM_PARSER_H

#include <Arduino.h>
#include <functional>
#include "config_manager.h"

class RTCMParser
{
public:
    typedef std::function<void(const uint8_t *data, size_t length, uint16_t messageType)> RTCMCallback;

    RTCMParser(ConfigManager &configMgr);
    void setCallback(RTCMCallback callback);
    void processData(const uint8_t *data, size_t length);
    bool shouldTransmitMessageType(uint16_t messageType);
    void updateMessageTypes(); // Update from config

private:
    ConfigManager &configManager;
    static const size_t BUFFER_SIZE = 4096;

    uint8_t buffer[BUFFER_SIZE];
    size_t bufferPos;
    RTCMCallback callback;

    // RTCM message type filtering
    bool allowedMessageTypes[4096]; // Support message types 0-4095

    void initializeAllowedMessageTypes();
    bool findAndProcessRTCMMessage();
    uint16_t extractMessageType(const uint8_t *data);
    uint32_t calculateCRC24(const uint8_t *data, size_t length);
    void shiftBuffer(size_t positions);
};

#endif