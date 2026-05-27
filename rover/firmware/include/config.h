#ifndef CONFIG_H
#define CONFIG_H

// ===== UART Configuration (GPS) =====
#define GPS_UART_NUM UART_NUM_1 // UART1 for GPS
#define GPS_RX_PIN 25           // GPIO25 - receive data from GPS (TX GPS → RX ESP32)
#define GPS_TX_PIN 27           // GPIO27 - send data to GPS (RX GPS ← TX ESP32)
#define GPS_BAUD_RATE 115200    // Standard baud rate for GPS modules
#define UART_BUF_SIZE 1024      // UART buffer size (1KB)

// ===== Debug UART Configuration =====
#define DEBUG_UART_NUM UART_NUM_0 // UART0 for debug/logging (Serial - USB)
#define DEBUG_BAUD_RATE 115200    // Baud rate for debug

// ===== WiFi Configuration (for OTA updates) =====
#define WIFI_SSID "YourNetwork"               // WiFi network name
#define WIFI_PASSWORD "YourPassword" // WiFi password
#define OTA_HOSTNAME "ossrtk"                // Hostname for OTA
#define OTA_PASSWORD "admin"                 // OTA update password
#define WIFI_TIMEOUT_MS 10000                // WiFi connection timeout (10s)
#define WIFI_ACTIVE_TIME_MS 180000           // WiFi active time after boot (3 minutes)

// ===== NTP Configuration =====
#define NTP_SERVER "pool.ntp.org" // NTP server
#define GMT_OFFSET_SEC 3600       // GMT+1 offset (1 hour)
#define DAYLIGHT_OFFSET_SEC 3600  // Daylight saving time offset (1 hour)

// ===== Power Saving Configuration =====
#define BLE_TX_POWER ESP_PWR_LVL_N9 // BLE TX power: -12dBm (low power mode)
#define CPU_FREQ_ACTIVE 240         // CPU frequency when transferring data (MHz)

// ===== BLE Configuration =====
#define BLE_DEVICE_NAME "OSSRTK" // Name visible in BLE scanners
#define BLE_MTU_SIZE 185         // Maximum MTU (BLE supports up to ~185)
#define BLE_CHUNK_SIZE 180       // Data chunk size to send (smaller than MTU)

// Nordic UART Service UUIDs (standard for BLE compatibility)
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // BLE → ESP32
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 → BLE

// ===== Other Settings =====
#define BLE_STATUS_LED_PIN 23 // BLE status LED on GPIO23 (active HIGH)
#define BLE_LED_BLINK_MS 250  // LED blink period when not connected (250ms ON/OFF)
#define NMEA_MAX_LENGTH 82    // Maximum NMEA sentence length

#endif // CONFIG_H
