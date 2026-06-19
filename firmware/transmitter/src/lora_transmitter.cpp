#include "lora_transmitter.h"

// TTGO T-Beam LoRa pin definitions (using board's predefined pins)
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
// Use board's predefined LORA_RST (GPIO23)
#define LORA_DIO0 26

LoRaTransmitter::LoRaTransmitter(ConfigManager &configMgr) : configManager(configMgr), currentMessageId(0)
{
}

bool LoRaTransmitter::begin()
{
    SystemConfig &cfg = configManager.getConfig();

    // Set LoRa pins
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(cfg.loraFrequency))
    {
#ifdef DEBUG
        Serial.println("LoRa initialization failed!");
#endif
        return false;
    }

    // Configure LoRa parameters according to protocol specification
    LoRa.setSpreadingFactor(cfg.loraSpreadingFactor);
    LoRa.setSignalBandwidth((long)cfg.loraBandwidth);
    LoRa.setCodingRate4(cfg.loraCodingRate);
    LoRa.setTxPower(cfg.loraTxPower);
    LoRa.setSyncWord(cfg.loraSyncWord);
    LoRa.enableCrc();

    // Extend preamble for better short packet reception
    LoRa.setPreambleLength(12); // Default is 8, increase to 12 for better detection

#ifdef DEBUG
    Serial.println("LoRa initialized successfully");
    Serial.printf("Frequency: %.0f MHz\n", cfg.loraFrequency / 1E6);
    Serial.printf("Spreading Factor: %d\n", cfg.loraSpreadingFactor);
    Serial.printf("Bandwidth: %.0f kHz\n", cfg.loraBandwidth / 1E3);
    Serial.printf("Coding Rate: 4/%d\n", cfg.loraCodingRate);
    Serial.printf("TX Power: %d dBm\n", cfg.loraTxPower);
    Serial.printf("Sync Word: 0x%02X\n", cfg.loraSyncWord);
    Serial.printf("Preamble Length: 12 symbols\n");
#endif

    return true;
}

bool LoRaTransmitter::transmitMessage(const uint8_t *data, size_t length)
{
    if (!data || length == 0)
    {
        return false;
    }

    // Calculate number of packets needed
    uint8_t totalPackets = (length + MAX_DATA_PER_PACKET - 1) / MAX_DATA_PER_PACKET;

    if (totalPackets > 255)
    {
        Serial.printf("Message too large: %d bytes (max ~62.7 KB)\n", length);
        return false;
    }

#if DEBUG_LORA_PACKETS
    Serial.printf("Transmitting RTCM message: %d bytes in %d packets (ID: %d)\n",
                  length, totalPackets, currentMessageId);
#endif

    bool success = true;

    // Transmit each packet
    for (uint8_t packetNum = 0; packetNum < totalPackets; packetNum++)
    {
        size_t offset = packetNum * MAX_DATA_PER_PACKET;
        size_t remainingBytes = length - offset;
        size_t packetDataSize = min(remainingBytes, MAX_DATA_PER_PACKET);

        PacketHeader header;
        header.frameType = FRAME_TYPE_RTCM;
        header.messageId = currentMessageId;
        header.totalPackets = totalPackets;
        header.packetNumber = packetNum;
        header.dataLength = packetDataSize;

        if (!transmitPacket(header, data + offset))
        {
#if DEBUG_LORA_VERBOSE
            Serial.printf("Failed to transmit packet %d/%d\n", packetNum + 1, totalPackets);
#endif
            success = false;
            break;
        }

        // Small delay between packets to avoid overwhelming the receiver
        if (packetNum < totalPackets - 1)
        {
            delay(75);
        }
    }

    if (success)
    {
#if DEBUG_LORA_PACKETS
        Serial.printf("Successfully transmitted message ID %d\n", currentMessageId);
#endif
        incrementMessageId();
    }
    // Additional delay after full message transmission
    delay(75);
    return success;
}

bool LoRaTransmitter::transmitPacket(const PacketHeader &header, const uint8_t *data)
{
    // Prepare packet
    uint8_t packet[MAX_PACKET_SIZE];

    packet[0] = header.frameType;
    packet[1] = header.messageId;
    packet[2] = header.totalPackets;
    packet[3] = header.packetNumber;
    packet[4] = header.dataLength;

    // Copy data
    memcpy(packet + HEADER_SIZE, data, header.dataLength);

    size_t packetSize = HEADER_SIZE + header.dataLength;

    // Pad short packets for better reception reliability
    if (packetSize < MIN_PACKET_SIZE)
    {
        // Fill padding with zeros
        memset(packet + packetSize, 0x00, MIN_PACKET_SIZE - packetSize);
        size_t originalSize = packetSize;
        packetSize = MIN_PACKET_SIZE;

#if DEBUG_LORA_VERBOSE
        Serial.printf("Padding packet from %d to %d bytes for better reception\n",
                      originalSize, packetSize);
#endif
    }

    // Transmit packet
    LoRa.beginPacket();
    LoRa.write(packet, packetSize);
    int result = LoRa.endPacket();

    if (result == 1)
    {
#if DEBUG_LORA_VERBOSE
        Serial.printf("Packet %d/%d sent (%d bytes)\n",
                      header.packetNumber + 1, header.totalPackets, packetSize);
#endif
        return true;
    }
    else
    {
#if DEBUG_LORA_VERBOSE
        Serial.printf("Failed to send packet %d/%d (error: %d)\n",
                      header.packetNumber + 1, header.totalPackets, result);
#endif
        return false;
    }
}

void LoRaTransmitter::incrementMessageId()
{
    currentMessageId = (currentMessageId + 1) & 0xFF; // Wrap around at 255
}