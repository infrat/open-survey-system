#ifndef LORA_TRANSMITTER_H
#define LORA_TRANSMITTER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config_manager.h"

// LoRa frame types (Protocol v2 — first byte of every packet)
enum LoRaFrameType : uint8_t
{
    FRAME_TYPE_RTCM        = 0x01, // RTCM correction data
    FRAME_TYPE_MAINTENANCE = 0x02, // Telemetry broadcast
    FRAME_TYPE_COMMAND     = 0x03, // Remote command (RX direction)
    FRAME_TYPE_ACK         = 0x04, // Acknowledgement
};

class LoRaTransmitter
{
public:
    LoRaTransmitter(ConfigManager &configMgr);
    bool begin();
    bool transmitMessage(const uint8_t *data, size_t length);

private:
    ConfigManager &configManager;
    static const size_t MAX_PACKET_SIZE = 250;
    static const size_t HEADER_SIZE = 5;                              // +1 for frameType
    static const size_t MAX_DATA_PER_PACKET = MAX_PACKET_SIZE - HEADER_SIZE; // 245
    static const size_t MIN_PACKET_SIZE = 32;

    uint8_t currentMessageId;

    struct PacketHeader
    {
        uint8_t frameType;    // LoRaFrameType
        uint8_t messageId;
        uint8_t totalPackets;
        uint8_t packetNumber;
        uint8_t dataLength;
    };

    bool transmitPacket(const PacketHeader &header, const uint8_t *data);
    void incrementMessageId();
};

#endif