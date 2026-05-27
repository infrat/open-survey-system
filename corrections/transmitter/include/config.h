#ifndef CONFIG_H
#define CONFIG_H

// Debug mode - comment out to disable debug output
#define DEBUG

// WiFi Configuration
#define WIFI_TIMEOUT_MS 30000

// NTRIP Configuration
#define NTRIP_HOST "91.198.76.2"
#define NTRIP_PORT 2101
#define NTRIP_MOUNTPOINT "RTN4G_VRS_RTCM32"
#define NTRIP_TIMEOUT_MS 10000
#define NTRIP_RECONNECT_INTERVAL_MS 30000

// GGA Configuration (Jaworzno, Poland)
#define GGA_LATITUDE 50.2346      // Jaworzno latitude
#define GGA_LONGITUDE 19.2084     // Jaworzno longitude
#define GGA_ALTITUDE 266.0        // Jaworzno altitude (approx.)
#define GGA_SEND_INTERVAL_MS 2000 // Send GGA every 10 seconds

// LoRa Configuration
#define LORA_FREQUENCY 433E6
#define LORA_SPREADING_FACTOR 7
#define LORA_BANDWIDTH 125E3
#define LORA_CODING_RATE 5
#define LORA_TX_POWER 20
#define LORA_SYNC_WORD 0x12

// Battery monitoring (always active, not configurable at runtime)
#define BATTERY_ADC_PIN         35      // ADC1_CH7 on T-Beam v1.1
#define BATTERY_SCALE_FACTOR    2.0f    // Voltage divider ratio

// Debug Configuration
#define DEBUG_LORA_PACKETS false
#define DEBUG_LORA_VERBOSE false // Detailed packet-by-packet logging
#define SERIAL_BAUD_RATE 115200

#endif