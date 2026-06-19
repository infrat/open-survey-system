#include <Arduino.h>
#include "config.h"
#include "debug.h"
#include "lora_radio.h"
#include "display.h"
#include "wifi_manager.h"
#include "ntrip_server.h"
#include "rtcm_parser.h"
#include "buzzer.h"

// Inter-core RTCM queue
QueueHandle_t rtcmQueue = NULL;

// LoRa task - runs on Core 1 (time-critical radio reception)
void loraTask(void *param)
{
  DEBUG_PRINTF("LoRa task started on core %d\n", xPortGetCoreID());

  for (;;)
  {
    // Check for LoRa packets
    int packetSize = checkForPackets();
    if (packetSize)
    {
      onReceive(packetSize);
    }

    // Check for message timeouts
    checkMessageTimeout();

    // Update buzzer (non-blocking pulse off)
    buzzerUpdate();

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

// Network task - runs on Core 0 (WiFi, NTRIP, display)
void networkTask(void *param)
{
  DEBUG_PRINTF("Network task started on core %d\n", xPortGetCoreID());
  unsigned long lastDisplayUpdate = 0;

  for (;;)
  {
    // Drain RTCM queue and broadcast to NTRIP clients
    RTCMQueueItem item;
    while (xQueueReceive(rtcmQueue, &item, 0) == pdTRUE)
    {
      broadcastRTCMData(item.data, item.length);
      delete[] item.data;
    }

    // Handle NTRIP clients
    handleNTRIPClients();

    // Handle WiFi clients
    handleWiFiClients();

    // Update display periodically
    if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL)
    {
      updateDisplay();
      lastDisplayUpdate = millis();
    }

    vTaskDelay(1 / portTICK_PERIOD_MS);
  }
}

void setup()
{
  initDebug();
  DEBUG_PRINTLN("LoRa NTRIP Server Starting...");

  // Initialize OLED display with delay
  setupOLED();

  // Show splash screen for 3 seconds
  showSplashScreen();
  delay(3000);

  // Initialize WiFi Hotspot
  setupWiFiHotspot();

  // Initialize NTRIP Server
  setupNTRIPServer();

  // Initialize RTCM Parser
  initRTCMBuffer();

  // Initialize buzzer
  setupBuzzer();

  // Initialize LoRa
  setupLoRa();

  // Send CMD_START 3x to trigger transmission on the transmitter side
  DEBUG_PRINTLN("Sending CMD_START x3...");
  for (int i = 0; i < 3; i++)
  {
    sendCommandFrame(0x07); // CMD_START
    delay(200);
  }

  // Create inter-core RTCM queue
  rtcmQueue = xQueueCreate(RTCM_QUEUE_SIZE, sizeof(RTCMQueueItem));
  if (!rtcmQueue)
  {
    DEBUG_PRINTLN("ERROR: Failed to create RTCM queue!");
    while (1)
      ;
  }

  DEBUG_PRINTLN("LoRa NTRIP Server Ready! Starting dual-core tasks...");
  updateDisplay();

  // Launch LoRa task on Core 1
  xTaskCreatePinnedToCore(loraTask, "LoRa", LORA_TASK_STACK, NULL, LORA_TASK_PRIORITY, NULL, LORA_CORE);

  // Launch Network task on Core 0
  xTaskCreatePinnedToCore(networkTask, "Network", NETWORK_TASK_STACK, NULL, NETWORK_TASK_PRIORITY, NULL, NETWORK_CORE);
}

void loop()
{
  // Arduino loop() is unused - work is done in FreeRTOS tasks
  vTaskDelete(NULL);
}