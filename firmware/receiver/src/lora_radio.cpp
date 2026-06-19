#include "lora_radio.h"
#include "debug.h"
#include "config.h"
#include "display.h"
#include "rtcm_parser.h"
#include "buzzer.h"
#include <SPI.h>
#include <map>

// Spinlock for statistics accessed from both cores
static portMUX_TYPE statsMux = portMUX_INITIALIZER_UNLOCKED;

// Reception statistics
static uint32_t completeMessagesReceived = 0;
static uint32_t timeoutMessages = 0;
static uint32_t crcErrorMessages = 0;             // RTCM messages with CRC errors
static int lastRssi = -999;                       // Last received packet RSSI in dBm
static unsigned long lastCompleteMessageTime = 0; // Timestamp of last complete message (millis)

// Message rate tracking (for last 10 seconds)
#include <deque>
static std::deque<unsigned long> messageTimestamps; // Timestamps of complete messages

// Message reassembly structures
struct PacketData
{
    uint8_t *data;
    uint8_t length;
    unsigned long timestamp;
};

struct MessageReassembly
{
    uint8_t messageId;
    uint8_t totalPackets;
    uint8_t receivedPackets;
    std::map<uint8_t, PacketData> packets; // packetNumber -> PacketData
    unsigned long lastPacketTime;
    bool isComplete;

    MessageReassembly() : messageId(0), totalPackets(0), receivedPackets(0),
                          lastPacketTime(0), isComplete(false) {}

    ~MessageReassembly()
    {
        // Clean up allocated packet data
        for (auto &pair : packets)
        {
            delete[] pair.second.data;
        }
        packets.clear();
    }
};

// Active message reassembly buffer (using message ID as key)
static std::map<uint8_t, MessageReassembly *> activeMessages;

void cleanupTimedOutMessages()
{
    unsigned long currentTime = millis();
    auto it = activeMessages.begin();

    while (it != activeMessages.end())
    {
        MessageReassembly *msg = it->second;

        if (currentTime - msg->lastPacketTime > PACKET_TIMEOUT)
        {
            DEBUG_PRINTF("Message %u timed out (%u/%u packets received)\n",
                         msg->messageId, msg->receivedPackets, msg->totalPackets);
            portENTER_CRITICAL(&statsMux);
            timeoutMessages++;
            portEXIT_CRITICAL(&statsMux);

            delete msg; // This will call destructor and clean up packet data
            it = activeMessages.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void resetMessageBuffer()
{
    // Clean up all active messages
    for (auto &pair : activeMessages)
    {
        delete pair.second;
    }
    activeMessages.clear();
}

void setupLoRa()
{
    DEBUG_PRINTLN("Initializing LoRa...");

    // Setup LoRa transceiver module
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RESET, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY))
    {
        DEBUG_PRINTLN("Starting LoRa failed!");
        showError("LoRa Init Failed!");
        while (1)
            ;
    }

    // Configure LoRa parameters according to protocol
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setPreambleLength(12);
    LoRa.enableCrc();

    // Initialize message buffer
    resetMessageBuffer();

    DEBUG_PRINTLN("LoRa initialized successfully!");
    DEBUG_PRINTF("Frequency: %.1f MHz\n", LORA_FREQUENCY / 1E6);
    DEBUG_PRINTF("Spreading Factor: %d\n", LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("Bandwidth: %.1f kHz\n", LORA_BANDWIDTH / 1E3);
    DEBUG_PRINTF("Sync Word: 0x%02X\n", LORA_SYNC_WORD);
}

void processCompleteMessage(MessageReassembly *message)
{
    // Calculate total message size
    uint16_t totalSize = 0;
    for (uint8_t i = 0; i < message->totalPackets; i++)
    {
        auto it = message->packets.find(i);
        if (it != message->packets.end())
        {
            totalSize += it->second.length;
        }
        else
        {
            DEBUG_PRINTF("ERROR: Missing packet %u in complete message!\n", i);
            return;
        }
    }

    // Allocate buffer for reassembled message
    uint8_t *reassembledData = new uint8_t[totalSize];
    if (!reassembledData)
    {
        DEBUG_PRINTLN("ERROR: Failed to allocate memory for reassembled message!");
        return;
    }

    // Reassemble packets in correct order
    uint16_t offset = 0;
    for (uint8_t i = 0; i < message->totalPackets; i++)
    {
        auto it = message->packets.find(i);
        if (it != message->packets.end())
        {
            memcpy(reassembledData + offset, it->second.data, it->second.length);
            offset += it->second.length;
        }
    }

    // Process the complete RTCM data
    processRTCMData(reassembledData, totalSize);

    // Clean up
    delete[] reassembledData;

    // Update statistics
    portENTER_CRITICAL(&statsMux);
    completeMessagesReceived++;
    lastCompleteMessageTime = millis();
    messageTimestamps.push_back(millis());
    portEXIT_CRITICAL(&statsMux);
}

void processRTCMData(const uint8_t *data, size_t length)
{
    // Add to RTCM buffer for reconstruction
    addToRTCMBuffer(data, length);

    // Process any complete RTCM messages from the buffer
    RTCMMessage rtcmMsg;
    do
    {
        rtcmMsg = parseNextRTCMMessage();

        if (rtcmMsg.crcError)
        {
            // Count messages with CRC errors
            portENTER_CRITICAL(&statsMux);
            crcErrorMessages++;
            portEXIT_CRITICAL(&statsMux);
        }

        if (rtcmMsg.valid)
        {
            DEBUG_PRINTF("RTCM Message: Type %u, Size %u bytes\n", rtcmMsg.messageType, rtcmMsg.length + 6);

            // Beep on valid RTCM frame
            buzzerBeep();

            // Enqueue complete RTCM message for Core 0 to broadcast
            RTCMQueueItem item;
            item.length = rtcmMsg.length + 6; // +6 for header and CRC
            item.data = new uint8_t[item.length];
            if (item.data)
            {
                memcpy(item.data, rtcmMsg.data, item.length);
                if (xQueueSend(rtcmQueue, &item, 0) != pdTRUE)
                {
                    DEBUG_PRINTLN("RTCM queue full, dropping message");
                    delete[] item.data;
                }
            }

            // Clean up allocated memory
            delete[] rtcmMsg.data;
        }
    } while (rtcmMsg.valid || rtcmMsg.crcError); // Continue if we found something (valid or CRC error)
}

void onReceive(int packetSize)
{
    DEBUG_PRINTF("-------------------------------\n");
    DEBUG_PRINTF("Received packet size: %d \n", packetSize);
    // Validate packet size according to protocol
    if (packetSize < HEADER_SIZE)
    {
        DEBUG_PRINTF("Packet too small: %d bytes (minimum %d)\n", packetSize, HEADER_SIZE);
        return;
    }

    if (packetSize > 250)
    { // Protocol maximum
        DEBUG_PRINTF("Packet too large: %d bytes (maximum 250)\n", packetSize);
        return;
    }

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();
    lastRssi = rssi; // Update global last RSSI value

    // Read and validate packet header (Protocol v2: 5 bytes)
    uint8_t frameType = LoRa.read();
    uint8_t messageId = LoRa.read();
    uint8_t totalPackets = LoRa.read();
    uint8_t packetNumber = LoRa.read();
    uint8_t dataLength = LoRa.read();

    // Handle non-RTCM frame types — drain remaining bytes and return
    if (frameType != FRAME_RTCM)
    {
        if (frameType == FRAME_MAINTENANCE)
        {
            DEBUG_PRINTF("RX: FRAME_MAINTENANCE received (%d bytes), ignoring\n", packetSize);
        }
        else
        {
            DEBUG_PRINTF("RX: Unknown frameType=0x%02X (%d bytes), ignoring\n", frameType, packetSize);
        }
        while (LoRa.available())
            LoRa.read();
        return;
    }

    if (packetNumber >= totalPackets || totalPackets == 0 || dataLength > MAX_DATA_SIZE)
    {
        static uint32_t protocolErrors = 0;
        protocolErrors++;
        if (protocolErrors % 10 == 0)
        {
            DEBUG_PRINTF("Protocol validation errors: %u\n", protocolErrors);
        }
        return;
    }

    // Log packet reception
    DEBUG_PRINTF("RX: ID=%u, Pkt=%u/%u, RSSI=%d dBm [#%u]\n",
                 messageId, packetNumber + 1, totalPackets, rssi);

    // Read packet data
    uint8_t *packetData = nullptr;
    if (dataLength > 0)
    {
        packetData = new uint8_t[dataLength];
        if (!packetData)
        {
            DEBUG_PRINTLN("ERROR: Failed to allocate memory for packet data!");
            return;
        }

        for (uint8_t i = 0; i < dataLength; i++)
        {
            packetData[i] = LoRa.read();
        }
    }

    // Find or create message reassembly structure
    MessageReassembly *message = nullptr;
    auto it = activeMessages.find(messageId);

    if (it != activeMessages.end())
    {
        message = it->second;

        // Validate message consistency
        if (message->totalPackets != totalPackets)
        {
            DEBUG_PRINTF("Packet count mismatch for message %u\n", messageId);
            // Clean up inconsistent message and start fresh
            delete message;
            activeMessages.erase(it);
            message = nullptr;
        }
    }

    if (!message)
    {
        // Create new message reassembly structure
        message = new MessageReassembly();
        if (!message)
        {
            DEBUG_PRINTLN("ERROR: Failed to allocate memory for message reassembly!");
            delete[] packetData;
            return;
        }

        message->messageId = messageId;
        message->totalPackets = totalPackets;
        message->receivedPackets = 0;
        message->lastPacketTime = millis();
        message->isComplete = false;

        activeMessages[messageId] = message;
    }

    // Check for duplicate packet
    auto packetIt = message->packets.find(packetNumber);
    if (packetIt != message->packets.end())
    {
        // Only log duplicate packets occasionally
        static uint32_t duplicateCount = 0;
        duplicateCount++;
        DEBUG_PRINTF("Duplicate packets: %u\n", duplicateCount);
        delete[] packetData;
        return;
    }

    // Store packet data
    PacketData pData;
    pData.data = packetData;
    pData.length = dataLength;
    pData.timestamp = millis();

    message->packets[packetNumber] = pData;
    message->receivedPackets++;
    message->lastPacketTime = millis();

    // Only log progress for multi-packet messages
    if (message->totalPackets > 1)
    {
        DEBUG_PRINTF("Progress: %u/%u packets for message %u\n",
                     message->receivedPackets, message->totalPackets, messageId);
    }

    // Check if message is complete
    if (message->receivedPackets == message->totalPackets)
    {
        DEBUG_PRINTF("Message %u complete!\n", messageId);
        // Process the complete message
        processCompleteMessage(message);

        // Clean up completed message
        delete message;
        activeMessages.erase(messageId);
    }

    // Periodic cleanup of timed-out messages
    static unsigned long lastCleanup = 0;
    if (millis() - lastCleanup > 1000)
    { // Cleanup every second
        cleanupTimedOutMessages();
        lastCleanup = millis();
    }
}

void checkMessageTimeout()
{
    cleanupTimedOutMessages();
}

void sendCommandFrame(uint8_t commandId)
{
    // Command frame payload: [commandId 1B][payloadLen 1B]
    static uint8_t msgIdCounter = 0;
    uint8_t payload[2] = {commandId, 0x00};

    LoRa.beginPacket();
    LoRa.write(FRAME_COMMAND);   // frameType
    LoRa.write(msgIdCounter++);  // msgId
    LoRa.write(0x01);            // totalPackets
    LoRa.write(0x00);            // packetNumber
    LoRa.write(sizeof(payload)); // dataLen
    LoRa.write(payload, sizeof(payload));
    LoRa.endPacket();

    DEBUG_PRINTF("TX: FRAME_COMMAND cmdId=0x%02X\n", commandId);
}

int checkForPackets()
{
    return LoRa.parsePacket();
}

// Helper function to clean up old timestamps (older than 10 seconds)
void cleanupOldTimestamps()
{
    unsigned long currentTime = millis();
    while (!messageTimestamps.empty())
    {
        unsigned long oldestTime = messageTimestamps.front();
        if (currentTime - oldestTime > 10000) // 10 seconds in milliseconds
        {
            messageTimestamps.pop_front();
        }
        else
        {
            break; // Timestamps are in order, so we can stop
        }
    }
}

// Getter functions for statistics (thread-safe)
uint32_t getCompleteMessagesReceived()
{
    portENTER_CRITICAL(&statsMux);
    uint32_t v = completeMessagesReceived;
    portEXIT_CRITICAL(&statsMux);
    return v;
}
uint32_t getTimeoutMessages()
{
    portENTER_CRITICAL(&statsMux);
    uint32_t v = timeoutMessages;
    portEXIT_CRITICAL(&statsMux);
    return v;
}
uint32_t getCrcErrorMessages()
{
    portENTER_CRITICAL(&statsMux);
    uint32_t v = crcErrorMessages;
    portEXIT_CRITICAL(&statsMux);
    return v;
}

// Get last received packet RSSI
int getLastRssi()
{
    portENTER_CRITICAL(&statsMux);
    int v = lastRssi;
    portEXIT_CRITICAL(&statsMux);
    return v;
}

// Get average message rate (messages per second) over last 10 seconds
float getMessageRate()
{
    portENTER_CRITICAL(&statsMux);
    cleanupOldTimestamps();

    if (messageTimestamps.empty())
    {
        portEXIT_CRITICAL(&statsMux);
        return 0.0f;
    }

    unsigned long currentTime = millis();
    unsigned long oldestTime = messageTimestamps.front();
    unsigned long timeSpan = currentTime - oldestTime;
    size_t count = messageTimestamps.size();
    portEXIT_CRITICAL(&statsMux);

    if (timeSpan < 10000)
    {
        if (timeSpan == 0)
            return 0.0f;
        return (float)count / (timeSpan / 1000.0f);
    }

    return (float)count / 10.0f;
}

// Get seconds since last complete message
uint32_t getSecondsSinceLastMessage()
{
    portENTER_CRITICAL(&statsMux);
    unsigned long t = lastCompleteMessageTime;
    portEXIT_CRITICAL(&statsMux);

    if (t == 0)
    {
        return 0;
    }

    unsigned long currentTime = millis();
    unsigned long elapsed = currentTime - t;
    return elapsed / 1000;
}

// Get error rate as percentage
float getErrorRate()
{
    portENTER_CRITICAL(&statsMux);
    uint32_t totalErrors = timeoutMessages + crcErrorMessages;
    uint32_t totalMessages = completeMessagesReceived + totalErrors;
    portEXIT_CRITICAL(&statsMux);

    if (totalMessages == 0)
    {
        return 0.0f;
    }

    return (float)totalErrors / (float)totalMessages * 100.0f;
}