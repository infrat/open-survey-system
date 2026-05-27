#include "rtcm_parser.h"

RTCMParser::RTCMParser(ConfigManager &configMgr) : configManager(configMgr), bufferPos(0)
{
    initializeAllowedMessageTypes();
}

void RTCMParser::updateMessageTypes()
{
    initializeAllowedMessageTypes();
}

void RTCMParser::setCallback(RTCMCallback cb)
{
    callback = cb;
}

void RTCMParser::processData(const uint8_t *data, size_t length)
{
    // Limit processing to prevent stack overflow and watchdog timeout
    size_t maxProcessPerCall = 256; // Process max 256 bytes at once
    size_t processed = 0;

    while (processed < length)
    {
        size_t chunkSize = min(maxProcessPerCall, length - processed);

        for (size_t i = 0; i < chunkSize; i++)
        {
            // Add byte to buffer
            if (bufferPos < BUFFER_SIZE)
            {
                buffer[bufferPos++] = data[processed + i];
            }
            else
            {
                // Buffer full, shift and add
                shiftBuffer(1);
                buffer[bufferPos - 1] = data[processed + i];
            }
        }

        // Try to find and process RTCM messages (limit iterations)
        int maxIterations = 10; // Prevent infinite loops
        while (findAndProcessRTCMMessage() && maxIterations-- > 0)
        {
            // Keep processing until no more complete messages or limit reached
            yield(); // Feed watchdog
        }

        processed += chunkSize;

        // Yield control to prevent watchdog timeout
        if (processed < length)
        {
            yield();
        }
    }
}

bool RTCMParser::shouldTransmitMessageType(uint16_t messageType)
{
    // Check if message type is allowed
    if (!allowedMessageTypes[messageType])
    {
        return false;
    }

    // Apply transmission ratio
    return true;
}

void RTCMParser::initializeAllowedMessageTypes()
{
    // Initialize all to false
    memset(allowedMessageTypes, false, sizeof(allowedMessageTypes));

    SystemConfig &cfg = configManager.getConfig();

    // Enable message types from config
    for (uint16_t type : cfg.rtcmMessageTypes)
    {
        allowedMessageTypes[type] = true;
    }

#ifdef DEBUG
    Serial.printf("RTCM message types initialized: %d types configured\n", cfg.rtcmMessageTypes.size());
#endif
}

bool RTCMParser::findAndProcessRTCMMessage()
{
    if (bufferPos < 3)
    {
        return false; // Need at least 3 bytes for header
    }

    // Look for RTCM3 preamble (0xD3)
    size_t preamblePos = 0;
    bool found = false;

    for (size_t i = 0; i <= bufferPos - 3; i++)
    {
        if (buffer[i] == 0xD3)
        {
            preamblePos = i;
            found = true;
            break;
        }
    }

    if (!found)
    {
        // No preamble found, keep only last 2 bytes (in case preamble is split)
        if (bufferPos > 2)
        {
            shiftBuffer(bufferPos - 2);
        }
        return false;
    }

    // Remove data before preamble
    if (preamblePos > 0)
    {
        shiftBuffer(preamblePos);
    }

    if (bufferPos < 3)
    {
        return false;
    }

    // Extract message length from header
    uint16_t messageLength = ((buffer[1] & 0x03) << 8) | buffer[2];
    uint16_t totalLength = 3 + messageLength + 3; // header + data + CRC

    // Check if we have the complete message
    if (bufferPos < totalLength)
    {
        return false; // Wait for more data
    }

    // Verify CRC
    uint32_t receivedCRC = (buffer[3 + messageLength] << 16) |
                           (buffer[3 + messageLength + 1] << 8) |
                           buffer[3 + messageLength + 2];

    uint32_t calculatedCRC = calculateCRC24(buffer, 3 + messageLength);

    if (receivedCRC != calculatedCRC)
    {
        Serial.println("RTCM CRC mismatch, skipping message");
        shiftBuffer(1); // Remove preamble and try again
        return true;    // Continue processing
    }

    // Extract message type
    uint16_t messageType = extractMessageType(buffer + 3);

    // Call callback for processing
    if (callback)
    {
        callback(buffer, totalLength, messageType);
    }

    // Remove processed message from buffer
    shiftBuffer(totalLength);

    return true; // Processed a message, check for more
}

uint16_t RTCMParser::extractMessageType(const uint8_t *data)
{
    // Message type is in the first 12 bits of the data payload
    return (data[0] << 4) | (data[1] >> 4);
}

uint32_t RTCMParser::calculateCRC24(const uint8_t *data, size_t length)
{
    // CRC-24Q polynomial: 0x1864CFB
    const uint32_t polynomial = 0x1864CFB;
    uint32_t crc = 0;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (uint32_t)data[i] << 16;

        for (int j = 0; j < 8; j++)
        {
            if (crc & 0x800000)
            {
                crc = (crc << 1) ^ polynomial;
            }
            else
            {
                crc <<= 1;
            }
            crc &= 0xFFFFFF; // Keep only 24 bits
        }
    }

    return crc;
}

void RTCMParser::shiftBuffer(size_t positions)
{
    if (positions >= bufferPos)
    {
        bufferPos = 0;
        return;
    }

    memmove(buffer, buffer + positions, bufferPos - positions);
    bufferPos -= positions;
}
