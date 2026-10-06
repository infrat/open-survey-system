#ifndef CONFIG_H
#define CONFIG_H

#define FIRMWARE_VERSION "2.0.0"

// ===== UART Configuration (GPS) =====
#define GPS_UART_NUM UART_NUM_1 // UART1 for GPS
#define GPS_RX_PIN 25           // GPIO25 - receive data from GPS (TX GPS → RX ESP32)
#define GPS_TX_PIN 27           // GPIO27 - send data to GPS (RX GPS ← TX ESP32)
#define GPS_BAUD_RATE 115200    // Standard baud rate for GPS modules
#define UART_BUF_SIZE 1024      // UART buffer size (1KB)

// ===== Debug UART Configuration =====
#define DEBUG_UART_NUM UART_NUM_0 // UART0 for debug/logging (Serial - USB)
#define DEBUG_BAUD_RATE 115200    // Baud rate for debug

// ===== Runtime settings: factory defaults =====
// Live values are stored in NVS and edited through the web UI. These are only
// used on first boot, after a reset, or when stored values fail validation.
#define DEFAULT_TRANSPORT_MODE 0  // 0 = BLE, 1 = Bluetooth SPP, 2 = WiFi TCP
#define DEFAULT_WIFI_MODE 2       // 0 = AP, 1 = STA, 2 = AP+STA
#define DEFAULT_SETUP_WINDOW_SEC 30 // WiFi-only setup window after boot, before Bluetooth starts
#define SETUP_WINDOW_MIN_SEC 15
#define SETUP_WINDOW_MAX_SEC 600

// ===== WiFi Configuration =====
#define WIFI_SSID "YourNetwork"               // default STA network name
#define WIFI_PASSWORD "YourPassword" // default STA password
#define AP_SSID_PREFIX "OSSRTK-"             // AP name = prefix + last MAC bytes
#define OTA_HOSTNAME "ossrtk"                // Hostname for OTA / mDNS
#define OTA_PASSWORD "admin"                 // OTA update password
#define WIFI_TIMEOUT_MS 10000                // STA connect timeout before AP fallback / service start

// ===== Network services =====
#define HTTP_PORT 80
#define TELNET_PORT 23
#define TCP_PORT_DEFAULT 10110 // de-facto standard NMEA-0183 over TCP port
#define TCP_MAX_CLIENTS 2

// ===== NTP Configuration =====
#define NTP_SERVER "pool.ntp.org" // NTP server
#define GMT_OFFSET_SEC 3600       // GMT+1 offset (1 hour)
#define DAYLIGHT_OFFSET_SEC 3600  // Daylight saving time offset (1 hour)

// ===== Power Saving Configuration =====
#define BLE_TX_POWER ESP_PWR_LVL_N9 // BLE TX power: -9dBm (low power mode)
#define SPP_TX_POWER ESP_PWR_LVL_N9 // BR/EDR TX power: -9dBm (low power mode)
#define CPU_FREQ_ACTIVE 240         // CPU frequency when transferring data (MHz)

// ===== Bluetooth Device Name (BLE and SPP) =====
#define BT_DEVICE_NAME "OSSRTK" // Name visible in BLE scanners / BT pairing list

// ===== BLE Configuration =====
#define BLE_MTU_SIZE 185   // Maximum MTU (BLE supports up to ~185)
#define BLE_CHUNK_SIZE 180 // Data chunk size to send (smaller than MTU)

// Nordic UART Service UUIDs (standard for BLE compatibility)
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // BLE → ESP32
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // ESP32 → BLE

// ===== Other Settings =====
#define LINK_STATUS_LED_PIN 23 // Transport status LED on GPIO23 (active HIGH)
#define LINK_LED_BLINK_MS 250  // LED blink period when not connected (250ms ON/OFF)
#define NMEA_MAX_LENGTH 82     // Maximum NMEA sentence length
#define BRIDGE_LOG_TRAFFIC 1   // Log every UART↔transport chunk (0 = quiet)

#endif // CONFIG_H
