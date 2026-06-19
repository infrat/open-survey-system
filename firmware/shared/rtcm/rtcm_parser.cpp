#include "rtcm_parser.h"
#include "debug.h"

RTCMBuffer rtcmBuffer;

void initRTCMBuffer()
{
    rtcmBuffer.writeIndex = 0;
    rtcmBuffer.readIndex = 0;
    rtcmBuffer.availableBytes = 0;
    rtcmBuffer.overflow = false;
    DEBUG_PRINTLN("RTCM buffer initialized");
}

bool addToRTCMBuffer(const uint8_t *data, size_t length)
{
    if (length == 0)
        return true;

    // Check if buffer has enough space
    size_t freeSpace = RTCM_BUFFER_SIZE - rtcmBuffer.availableBytes;
    if (length > freeSpace)
    {
        DEBUG_PRINTF("RTCM buffer overflow! Need %u bytes, have %u\n", length, freeSpace);
        rtcmBuffer.overflow = true;
        return false;
    }

    // Add data to circular buffer
    for (size_t i = 0; i < length; i++)
    {
        rtcmBuffer.data[rtcmBuffer.writeIndex] = data[i];
        rtcmBuffer.writeIndex = (rtcmBuffer.writeIndex + 1) % RTCM_BUFFER_SIZE;
    }

    rtcmBuffer.availableBytes += length;
    // Only log significant buffer additions
    if (length > 50)
    {
        DEBUG_PRINTF("RTCM buffer: +%u bytes (total: %u)\n", length, rtcmBuffer.availableBytes);
    }

    return true;
}

RTCMMessage parseNextRTCMMessage()
{
    RTCMMessage message;
    message.valid = false;
    message.crcError = false;
    message.data = nullptr;

    // Need at least 6 bytes for RTCM header (preamble + length + message type)
    if (rtcmBuffer.availableBytes < 6)
    {
        return message;
    }

    // Look for RTCM preamble (0xD3)
    uint16_t searchIndex = rtcmBuffer.readIndex;
    bool foundPreamble = false;

    for (uint16_t i = 0; i < rtcmBuffer.availableBytes; i++)
    {
        if (rtcmBuffer.data[searchIndex] == 0xD3)
        {
            foundPreamble = true;
            // Adjust read index to preamble position
            uint16_t skipped = (searchIndex - rtcmBuffer.readIndex + RTCM_BUFFER_SIZE) % RTCM_BUFFER_SIZE;
            rtcmBuffer.readIndex = searchIndex;
            rtcmBuffer.availableBytes -= skipped;
            if (skipped > 0)
            {
                DEBUG_PRINTF("Skipped %u bytes to find RTCM preamble\n", skipped);
            }
            break;
        }
        searchIndex = (searchIndex + 1) % RTCM_BUFFER_SIZE;
    }

    if (!foundPreamble)
    {
        // No preamble found - log occasionally for debugging
        static uint32_t noPreambleCount = 0;
        noPreambleCount++;

        // Only log every 20th failure to reduce spam
        if (noPreambleCount % 20 == 0)
        {
            DEBUG_PRINTF("No RTCM preamble found (failures: %u)\n", noPreambleCount);
        }

        // Don't clear entire buffer immediately - just remove first byte and try again
        if (rtcmBuffer.availableBytes > 0)
        {
            rtcmBuffer.readIndex = (rtcmBuffer.readIndex + 1) % RTCM_BUFFER_SIZE;
            rtcmBuffer.availableBytes--;
        }
        return message;
    }

    // Check if we have enough data for the header
    if (rtcmBuffer.availableBytes < 6)
    {
        return message; // Need more data
    }

    // Read RTCM header
    uint8_t header[6];
    for (int i = 0; i < 6; i++)
    {
        header[i] = rtcmBuffer.data[(rtcmBuffer.readIndex + i) % RTCM_BUFFER_SIZE];
    }

    // Parse length (bits 14-23 of header, after preamble)
    uint16_t length = ((header[1] & 0x03) << 8) | header[2];

    // Total message size = header(3) + length + CRC(3)
    uint16_t totalSize = 3 + length + 3;

    // Validate length
    if (length > 1023)
    { // RTCM max length
        DEBUG_PRINTF("Invalid RTCM length: %u\n", length);
        // Skip this byte and try again
        rtcmBuffer.readIndex = (rtcmBuffer.readIndex + 1) % RTCM_BUFFER_SIZE;
        rtcmBuffer.availableBytes--;
        return message;
    }

    // Check if complete message is available
    if (rtcmBuffer.availableBytes < totalSize)
    {
        return message; // Need more data
    }

    // Extract complete message
    uint8_t *completeMessage = new uint8_t[totalSize];

    for (uint16_t i = 0; i < totalSize; i++)
    {
        uint16_t bufferIndex = (rtcmBuffer.readIndex + i) % RTCM_BUFFER_SIZE;
        completeMessage[i] = rtcmBuffer.data[bufferIndex];
    }

    // Validate CRC
    if (!isValidRTCMMessage(completeMessage, totalSize))
    {
        static uint32_t crcErrorCount = 0;
        crcErrorCount++;

        // Only log CRC errors occasionally
        if (crcErrorCount % 10 == 0)
        {
            DEBUG_PRINTF("RTCM CRC errors: %u (latest: %u bytes)\n", crcErrorCount, totalSize);
        }

        delete[] completeMessage;

        // Skip the entire calculated message to find next valid message
        rtcmBuffer.readIndex = (rtcmBuffer.readIndex + totalSize) % RTCM_BUFFER_SIZE;
        rtcmBuffer.availableBytes -= totalSize;

        // Mark that we found a complete message structure but with invalid CRC
        message.crcError = true;
        return message;
    }

    // Parse message type (bits 0-11 of payload)
    message.messageType = (header[3] << 4) | (header[4] >> 4);
    message.length = length;
    message.data = completeMessage;
    message.valid = true;

    // Extract station ID (bits 12-23 of payload, if available)
    if (length >= 2)
    {
        message.stationId = ((header[4] & 0x0F) << 8) | header[5];
    }
    else
    {
        message.stationId = 0;
    }

    message.description = getRTCMMessageDescription(message.messageType);

    // Update buffer pointers
    rtcmBuffer.readIndex = (rtcmBuffer.readIndex + totalSize) % RTCM_BUFFER_SIZE;
    rtcmBuffer.availableBytes -= totalSize;

    // Log successful RTCM message parsing (but not too frequently)
    static uint32_t successCount = 0;
    successCount++;
    if (successCount % 10 == 0 || message.messageType == 1005 || message.messageType == 1006)
    {
        DEBUG_PRINTF("RTCM %u (%s): %u bytes [#%u]\n",
                     message.messageType, message.description.c_str(),
                     message.length + 6, successCount);
    }

    return message;
}

String getRTCMMessageDescription(uint16_t messageType)
{
    switch (messageType)
    {
    case 1001:
        return "GPS L1 Code";
    case 1002:
        return "GPS L1 Code+Phase";
    case 1003:
        return "GPS L1/L2 Code";
    case 1004:
        return "GPS L1/L2 Code+Phase";
    case 1005:
        return "Station Coordinates";
    case 1006:
        return "Station Coordinates+Height";
    case 1007:
        return "Antenna Description";
    case 1008:
        return "Antenna Serial Number";
    case 1009:
        return "GLONASS L1 Code";
    case 1010:
        return "GLONASS L1 Code+Phase";
    case 1011:
        return "GLONASS L1/L2 Code";
    case 1012:
        return "GLONASS L1/L2 Code+Phase";
    case 1013:
        return "System Parameters";
    case 1019:
        return "GPS Ephemeris";
    case 1020:
        return "GLONASS Ephemeris";
    case 1033:
        return "Receiver Information";
    case 1074:
        return "GPS MSM4";
    case 1075:
        return "GPS MSM5";
    case 1077:
        return "GPS MSM7";
    case 1084:
        return "GLONASS MSM4";
    case 1085:
        return "GLONASS MSM5";
    case 1087:
        return "GLONASS MSM7";
    case 1094:
        return "Galileo MSM4";
    case 1095:
        return "Galileo MSM5";
    case 1097:
        return "Galileo MSM7";
    case 1124:
        return "BeiDou MSM4";
    case 1125:
        return "BeiDou MSM5";
    case 1127:
        return "BeiDou MSM7";
    case 1230:
        return "GLONASS Code-Phase Bias";
    default:
        return "Unknown (" + String(messageType) + ")";
    }
}

bool isValidRTCMMessage(const uint8_t *data, size_t length)
{
    if (length < 6)
        return false; // Minimum RTCM message size

    // Check preamble
    if (data[0] != 0xD3)
        return false;

    // Calculate and verify CRC
    uint32_t calculatedCRC = calculateRTCMCRC(data, length - 3);

    uint32_t messageCRC = ((uint32_t)data[length - 3] << 16) |
                          ((uint32_t)data[length - 2] << 8) |
                          (uint32_t)data[length - 1];

    bool isValid = (calculatedCRC == messageCRC);

    // Only log CRC mismatches occasionally to reduce spam
    if (!isValid)
    {
        static uint32_t crcFailCount = 0;
        DEBUG_PRINTF("CRC failures: %u (calc=0x%06X, msg=0x%06X)\n",
                     crcFailCount, calculatedCRC, messageCRC);
    }

    return isValid;
}

uint32_t calculateRTCMCRC(const uint8_t *data, size_t length)
{
    // RTCM uses CRC-24Q polynomial: 0x1864CFB
    uint32_t crc = 0;
    uint32_t polynomial = 0x1864CFB;

    for (size_t i = 0; i < length; i++)
    {
        crc ^= (data[i] << 16);
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

void logRTCMMessage(const RTCMMessage &message)
{
    if (message.valid)
    {
        DEBUG_PRINTF("RTCM %u: %s (Station: %u, %u bytes)\n",
                     message.messageType,
                     message.description.c_str(),
                     message.stationId,
                     message.length + 6); // +6 for header and CRC
    }
}

size_t getBufferedRTCMData(uint8_t *buffer, size_t maxLength)
{
    size_t bytesToCopy = (rtcmBuffer.availableBytes < maxLength ? rtcmBuffer.availableBytes : maxLength);

    for (size_t i = 0; i < bytesToCopy; i++)
    {
        buffer[i] = rtcmBuffer.data[(rtcmBuffer.readIndex + i) % RTCM_BUFFER_SIZE];
    }

    // Update buffer state
    rtcmBuffer.readIndex = (rtcmBuffer.readIndex + bytesToCopy) % RTCM_BUFFER_SIZE;
    rtcmBuffer.availableBytes -= bytesToCopy;

    return bytesToCopy;
}

void clearRTCMBuffer()
{
    rtcmBuffer.writeIndex = 0;
    rtcmBuffer.readIndex = 0;
    rtcmBuffer.availableBytes = 0;
    rtcmBuffer.overflow = false;
}

void analyzeBufferContents()
{
    if (rtcmBuffer.availableBytes == 0)
    {
        DEBUG_PRINTLN("RTCM buffer is empty");
        return;
    }

    DEBUG_PRINTF("RTCM buffer: %u bytes available\n", rtcmBuffer.availableBytes);

    // Look for potential RTCM preambles
    int preambleCount = 0;
    for (uint16_t i = 0; i < rtcmBuffer.availableBytes; i++)
    {
        uint8_t byte = rtcmBuffer.data[(rtcmBuffer.readIndex + i) % RTCM_BUFFER_SIZE];
        if (byte == 0xD3)
        {
            preambleCount++;
            if (preambleCount <= 3) // Only show first 3 preambles
            {
                DEBUG_PRINTF("RTCM preamble at offset %u\n", i);
            }
        }
    }

    if (preambleCount == 0)
    {
        DEBUG_PRINTLN("No RTCM preambles found");
    }
    else
    {
        DEBUG_PRINTF("Found %d RTCM preambles\n", preambleCount);
    }
}
