#ifndef LORA_RADIO_H
#define LORA_RADIO_H

#include <LoRa.h>
#include "rtcm_parser.h"

// RTCM queue item for inter-core communication
struct RTCMQueueItem
{
    uint8_t *data;
    size_t length;
};

// Queue handle (created in main.cpp)
extern QueueHandle_t rtcmQueue;

// LoRa functions
void setupLoRa();
void onReceive(int packetSize);
int checkForPackets();
void checkMessageTimeout();
void sendCommandFrame(uint8_t commandId);

// Message processing functions
void processCompleteMessage(class MessageReassembly *message);
void processRTCMData(const uint8_t *data, size_t length);
void cleanupTimedOutMessages();
void resetMessageBuffer();

// Statistics functions (thread-safe via spinlock)
uint32_t getCompleteMessagesReceived();
uint32_t getTimeoutMessages();
uint32_t getCrcErrorMessages();
int getLastRssi();
float getMessageRate();
uint32_t getSecondsSinceLastMessage();
float getErrorRate();

#endif // LORA_RADIO_H