#ifndef RTCM_PARSER_H
#define RTCM_PARSER_H

#include <Arduino.h>
#include "config.h"

// RTCM message structure
struct RTCMMessage
{
    uint16_t messageType;
    uint16_t length;
    uint32_t stationId;
    uint8_t *data;
    bool valid;
    bool crcError; // true if complete message was found but had invalid CRC
    String description;
};

// RTCM buffer for accumulating complete messages
struct RTCMBuffer
{
    uint8_t data[RTCM_BUFFER_SIZE];
    uint16_t writeIndex;
    uint16_t readIndex;
    uint16_t availableBytes;
    bool overflow;
};

// RTCM parsing functions
void initRTCMBuffer();
bool addToRTCMBuffer(const uint8_t *data, size_t length);
RTCMMessage parseNextRTCMMessage();
String getRTCMMessageDescription(uint16_t messageType);
bool isValidRTCMMessage(const uint8_t *data, size_t length);
uint32_t calculateRTCMCRC(const uint8_t *data, size_t length);
void logRTCMMessage(const RTCMMessage &message);
size_t getBufferedRTCMData(uint8_t *buffer, size_t maxLength);
void clearRTCMBuffer();
void analyzeBufferContents();

// Global RTCM buffer
extern RTCMBuffer rtcmBuffer;

#endif // RTCM_PARSER_H