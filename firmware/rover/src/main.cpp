#include <Arduino.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <time.h>
#include "config.h"
#include "uart_handler.h"
#include "rover_transport.h"

// Telnet server for remote debugging
WiFiServer telnetServer(23);
WiFiClient telnetClient;

// Global objects
UARTHandler uartHandler;

// Mobile-facing transport: BLE NUS or Bluetooth Classic SPP, picked by
// ROVER_TRANSPORT in config.h (see rover_transport.h for the selection).
RoverTransport transport;

// Buffer for UART data
uint8_t uartBuffer[UART_BUF_SIZE];

// Power saving state
bool wifiEnabled = false;
unsigned long wifiStartTime = 0;
bool timeSynchronized = false;

// Get formatted timestamp string // https://mapy.geoportal.gov.pl/wss/service/PZGIK/ORTO/WMS/StandardResolution
// https://mapy.geoportal.gov.pl/wss/service/PZGIK/ORTO/WMS/HighResolution
String getTimestamp()
{
    if (!timeSynchronized)
    {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "[%010lu]", millis());
        return String(buffer);
    }

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "[%010lu]", millis());
        return String(buffer);
    }

    char buffer[32];
    strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", &timeinfo);
    return String(buffer);
}

// Helper function for logging to both Serial and Telnet
void logPrint(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    String timestamp = getTimestamp();
    String output = timestamp + " " + String(buffer);

    Serial.print(output);
    if (wifiEnabled && telnetClient && telnetClient.connected())
    {
        telnetClient.print(output);
    }
}

void logPrintln(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    String timestamp = getTimestamp();
    String output = timestamp + " " + String(buffer);

    Serial.println(output);
    if (wifiEnabled && telnetClient && telnetClient.connected())
    {
        telnetClient.println(output);
    }
}

// Callback invoked when data arrives from the mobile app (app → UART)
void onTransportDataReceived(const uint8_t *data, size_t length)
{
    logPrint("[Bridge] %s → UART: %d bytes\n", transport.name(), length);

    // Send data to GPS via UART
    uartHandler.writeData(data, length);
}

// Handle Telnet commands
void handleTelnetCommands()
{
    if (telnetClient && telnetClient.connected())
    {
        while (telnetClient.available())
        {
            String cmd = telnetClient.readStringUntil('\n');
            cmd.trim();

            if (cmd == "status")
            {
                logPrintln("[Status] %s: %s", transport.name(), transport.isConnected() ? "Connected" : "Disconnected");
                logPrintln("[Status] WiFi: %s", wifiEnabled ? "Enabled" : "Disabled");
                logPrintln("[Status] Free Heap: %d bytes", ESP.getFreeHeap());
                logPrintln("[Status] Uptime: %lu seconds", millis() / 1000);
            }
            else if (cmd == "restart")
            {
                logPrintln("[Telnet] Restarting ESP32...");
                delay(1000);
                ESP.restart();
            }
            else if (cmd == "help")
            {
                logPrintln("Available commands:");
                logPrintln("  status  - Show system status");
                logPrintln("  restart - Restart ESP32");
                logPrintln("  help    - Show this help");
            }
        }
    }
}

void setup()
{
    // Initialize Debug UART (UART0 - USB Serial)
    Serial.begin(DEBUG_BAUD_RATE);
    delay(1000);

    logPrintln("\n\n========================================");
    logPrintln("ESP32 UART-%s Bridge", transport.name());
#if ROVER_TRANSPORT == TRANSPORT_SPP
    logPrintln("GPS (NMEA) ↔ Android via Bluetooth Classic SPP");
#else
    logPrintln("GPS (NMEA) ↔ iOS/Android via Nordic UART Service");
#endif
    logPrintln("========================================\n");

    // Initialize link status LED
    pinMode(LINK_STATUS_LED_PIN, OUTPUT);
    digitalWrite(LINK_STATUS_LED_PIN, LOW); // LOW = OFF (active HIGH)

    // 1. Initialize WiFi for OTA and WebSerial
    logPrintln("[Setup] Connecting to WiFi...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    unsigned long wifiConnectStart = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - wifiConnectStart) < WIFI_TIMEOUT_MS)
    {
        delay(250);
        logPrint(".");
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        logPrintln("\n[WiFi] Connected!");
        logPrint("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());

        wifiEnabled = true;
        wifiStartTime = millis();
        logPrint("[WiFi] Will auto-disable after %d seconds if no Telnet clients\n", WIFI_ACTIVE_TIME_MS / 1000);

        // Synchronize time with NTP server
        logPrintln("[NTP] Synchronizing time...");
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

        // Wait up to 5 seconds for time sync
        int retries = 0;
        struct tm timeinfo;
        while (!getLocalTime(&timeinfo) && retries < 10)
        {
            delay(500);
            retries++;
        }

        if (getLocalTime(&timeinfo))
        {
            timeSynchronized = true;
            char timeStr[64];
            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
            logPrint("[NTP] Time synchronized: %s\n", timeStr);
        }
        else
        {
            logPrintln("[NTP] Time sync failed - using millis() timestamps");
        }

        // Configure OTA
        ArduinoOTA.setHostname(OTA_HOSTNAME);
        ArduinoOTA.setPassword(OTA_PASSWORD);

        ArduinoOTA.onStart([]()
                           {
            String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
            logPrintln("[OTA] Update started: %s", type.c_str()); });

        ArduinoOTA.onEnd([]()
                         { logPrintln("\n[OTA] Update complete!"); });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                              { logPrint("[OTA] Progress: %u%%\r", (progress / (total / 100))); });

        ArduinoOTA.onError([](ota_error_t error)
                           {
            logPrint("[OTA] Error[%u]: ", error);
            if (error == OTA_AUTH_ERROR) logPrintln("Auth Failed");
            else if (error == OTA_BEGIN_ERROR) logPrintln("Begin Failed");
            else if (error == OTA_CONNECT_ERROR) logPrintln("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR) logPrintln("Receive Failed");
            else if (error == OTA_END_ERROR) logPrintln("End Failed"); });

        ArduinoOTA.begin();
        logPrintln("[OTA] Ready for firmware updates");
        logPrint("[OTA] Hostname: %s.local\n", OTA_HOSTNAME);

        // Start Telnet server
        telnetServer.begin();
        telnetServer.setNoDelay(true);
        logPrintln("[Telnet] Server started on port 23");
        logPrint("[Telnet] Connect with: telnet %s.local\n", OTA_HOSTNAME);
        logPrint("[Telnet] Or: telnet %s\n", WiFi.localIP().toString().c_str());
    }
    else
    {
        logPrintln("\n[WiFi] Connection failed - continuing without OTA/Telnet");
    }

    // 2. Initialize UART for GPS
    logPrintln("[Setup] Initializing UART...");

    // Set logging callback so UART handler can use logPrintln
    uartHandler.setLogCallback(logPrintln);

    if (!uartHandler.begin())
    {
        logPrintln("[ERROR] UART initialization failed!");
        while (1)
        {
            // Fast blinking on error
            digitalWrite(LINK_STATUS_LED_PIN, !digitalRead(LINK_STATUS_LED_PIN));
            delay(100);
        }
    }

    // 3. Initialize the mobile-facing transport (BLE NUS or Bluetooth SPP)
    logPrintln("[Setup] Initializing %s...", transport.name());

    // Set logging callback so the transport can use logPrintln
    transport.setLogCallback(logPrintln);

    if (!transport.begin(BT_DEVICE_NAME))
    {
        logPrintln("[ERROR] %s initialization failed!", transport.name());
        while (1)
        {
            // Fast blinking on error
            digitalWrite(LINK_STATUS_LED_PIN, !digitalRead(LINK_STATUS_LED_PIN));
            delay(200);
        }
    }

    // Radio-specific low power TX level (see BLE_TX_POWER / SPP_TX_POWER)
    transport.setTxPower();

    // Set callback for incoming data from the mobile app
    transport.setDataCallback(onTransportDataReceived);

    logPrintln("\n[Setup] Initialization complete!");
    logPrintln("[Setup] Bridge is ready to forward data:");
    logPrintln("         GPS (UART) → %s → mobile", transport.name());
    logPrintln("         mobile → %s → GPS (UART)", transport.name());
    logPrintln("\nWaiting for GPS data and mobile connection...\n");

    // LED OFF on start - waiting for connection
    digitalWrite(LINK_STATUS_LED_PIN, LOW); // LOW = OFF
}

void loop()
{
    // ===== Handle Telnet connections =====
    if (wifiEnabled)
    {
        // Check for new Telnet clients
        if (telnetServer.hasClient())
        {
            // Disconnect old client if exists
            if (telnetClient && telnetClient.connected())
            {
                telnetClient.stop();
            }

            telnetClient = telnetServer.available();
            logPrintln("[Telnet] Client connected from %s", telnetClient.remoteIP().toString().c_str());
            telnetClient.println("=== OSS RTK Rover - ESP32 Debug Console ===");
            telnetClient.println("Type 'help' for available commands\n");

            // Reset WiFi timer when client connects
            wifiStartTime = millis();
        }

        // Handle Telnet commands
        handleTelnetCommands();
    }

    // ===== WiFi Auto-Shutdown (only if no Telnet clients) =====
    if (wifiEnabled && (millis() - wifiStartTime) >= WIFI_ACTIVE_TIME_MS)
    {
        // Check if Telnet client is connected
        if (!telnetClient || !telnetClient.connected())
        {
            logPrintln("[WiFi] Auto-shutdown after 3 minutes - no Telnet clients");
            logPrintln("[Power] Disabling WiFi to save ~100mA");
            logPrintln("[Info] Reset device to re-enable WiFi for OTA/Telnet");

            if (telnetClient)
            {
                telnetClient.stop();
            }
            telnetServer.stop();
            ArduinoOTA.end();
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            wifiEnabled = false;
        }
        else
        {
            // Reset timer if client is connected
            wifiStartTime = millis();
        }
    }

    // Handle OTA updates (only if WiFi is enabled)
    if (wifiEnabled)
    {
        ArduinoOTA.handle();
    }

    // ===== UART → transport (GPS → mobile) =====
    // Check if there's GPS data and send IMMEDIATELY
    size_t availableBytes = uartHandler.available();
    if (availableBytes > 0)
    {
        // Read as much as available (don't wait for full buffer)
        size_t bytesToRead = min(availableBytes, (size_t)(UART_BUF_SIZE - 1));
        int bytesRead = uartHandler.readData(uartBuffer, bytesToRead);

        if (bytesRead > 0)
        {
            // Debug log
            logPrint("[Bridge] UART → %s: %d bytes\n", transport.name(), bytesRead);

            // SEND IMMEDIATELY (BLE chunking / SPP queueing is in sendData)
            if (transport.isConnected())
            {
                transport.sendData(uartBuffer, bytesRead);
            }
        }
    }

    // ===== Link Status LED =====
    static unsigned long lastBlink = 0;
    static bool ledState = false;

    if (transport.isConnected())
    {
        // Device connected - LED solid (HIGH = ON)
        digitalWrite(LINK_STATUS_LED_PIN, HIGH);
    }
    else
    {
        // No connection - LED blinks every 250ms
        if (millis() - lastBlink >= LINK_LED_BLINK_MS)
        {
            ledState = !ledState;
            digitalWrite(LINK_STATUS_LED_PIN, ledState ? HIGH : LOW); // HIGH = ON, LOW = OFF
            lastBlink = millis();
        }
    }

    // Small delay for stability
    delay(10);
}
