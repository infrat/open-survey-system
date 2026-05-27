#ifndef CONFIG_H
#define CONFIG_H

// LoRa pin definitions for TTGO LoRa32 V2.1
#define LORA_SCK 5
#define LORA_MISO 19
#define LORA_MOSI 27
#define LORA_SS 18
#define LORA_RESET 14
#define LORA_DIO0 26

// OLED pin definitions for TTGO LoRa32 V2.1
#define OLED_SDA 21
#define OLED_SCL 22
#define OLED_RST 16

// LoRa configuration (from protocol documentation)
#define LORA_FREQUENCY 444200000
#define LORA_SPREADING_FACTOR 7
#define LORA_BANDWIDTH 500E3
#define LORA_CODING_RATE 5
#define LORA_TX_POWER 20
#define LORA_SYNC_WORD 0x12

// Protocol v2 frame types
#define FRAME_RTCM        0x01  // RTCM correction data
#define FRAME_MAINTENANCE 0x02  // Telemetry broadcast (TX → air)
#define FRAME_COMMAND     0x03  // Control command (RX → transmitter)
#define FRAME_ACK         0x04  // Optional acknowledgement

// Protocol constants
#define MAX_PACKET_SIZE 512
#define HEADER_SIZE 5           // Protocol v2: frameType + msgID + totalPkts + pktNum + dataLen
#define MAX_DATA_SIZE 245       // 250 byte max LoRa frame − 5 byte header
#define MAX_MESSAGE_BUFFER 16384 // 16KB buffer for reassembling messages

// Buzzer configuration
#define BUZZER_PIN 4
#define BUZZER_PULSE_US 1000 // Buzzer pulse duration in microseconds

// Debug configuration
#define ENABLE_UART_LOGGING true // Set to false to disable all UART logging

// Timing constants
#define DISPLAY_UPDATE_INTERVAL 500 // Update every 500ms
#define PACKET_TIMEOUT 5000         // 5 second timeout for incomplete messages

// WiFi Hotspot configuration
#define WIFI_SSID "OSS-LoRa-RX"
#define WIFI_PASSWORD "lora123456"
#define WIFI_CHANNEL 1
#define WIFI_MAX_CONNECTIONS 4

// NTRIP Server configuration
#define NTRIP_PORT 2101
#define NTRIP_MOUNT_POINT "LORA"
#define NTRIP_MAX_CLIENTS 4
#define RTCM_BUFFER_SIZE 8192

// Dual-core configuration
#define LORA_CORE 1    // Core for LoRa reception & RTCM parsing
#define NETWORK_CORE 0 // Core for WiFi, NTRIP, display
#define LORA_TASK_STACK 8192
#define NETWORK_TASK_STACK 8192
#define LORA_TASK_PRIORITY 2 // Higher priority for time-critical LoRa
#define NETWORK_TASK_PRIORITY 1
#define RTCM_QUEUE_SIZE 16 // Max queued RTCM messages between cores

#endif // CONFIG_H