#include "wifi_manager.h"

WiFiManager::WiFiManager(ConfigManager &configMgr) : configManager(configMgr), lastConnectionAttempt(0)
{
}

bool WiFiManager::connect()
{
    if (WiFi.status() == WL_CONNECTED)
    {
        return true;
    }

    // For initial connection, always attempt. For subsequent reconnections, check retry interval.
    if (lastConnectionAttempt > 0 && millis() - lastConnectionAttempt < CONNECTION_RETRY_INTERVAL)
    {
        return false;
    }

    lastConnectionAttempt = millis();

    SystemConfig &cfg = configManager.getConfig();

#ifdef DEBUG
    Serial.println("Connecting to WiFi...");
    Serial.print("SSID: ");
    Serial.println(cfg.wifiSSID);

    // First, scan for networks to see if our SSID is available
    Serial.println("Scanning for WiFi networks...");
#endif

    // Disconnect first to ensure clean state
    WiFi.disconnect();
    delay(100);

    // Set WiFi mode to AP+STA (preserve AP if running)
    WiFi.mode(WIFI_MODE_APSTA);
    delay(100);
    WiFi.begin(cfg.wifiSSID.c_str(), cfg.wifiPassword.c_str());

    unsigned long startTime = millis();
    int dotCount = 0;

    while (WiFi.status() != WL_CONNECTED && millis() - startTime < cfg.wifiTimeout)
    {
        delay(500);
        Serial.print(".");
        dotCount++;

        // Print status every 10 dots for debugging
        if (dotCount % 10 == 0)
        {
            Serial.print(" [Status: ");
            Serial.print(WiFi.status());
            Serial.print("] ");
        }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("WiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        Serial.print("RSSI: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
        return true;
    }
    else
    {
        Serial.print("WiFi connection failed! Status: ");
        Serial.println(WiFi.status());

        // Print detailed status information
        switch (WiFi.status())
        {
        case WL_NO_SSID_AVAIL:
            Serial.println("Error: SSID not found");
            break;
        case WL_CONNECT_FAILED:
            Serial.println("Error: Connection failed (wrong password?)");
            break;
        case WL_CONNECTION_LOST:
            Serial.println("Error: Connection lost");
            break;
        case WL_DISCONNECTED:
            Serial.println("Error: Disconnected");
            break;
        default:
            Serial.println("Error: Unknown WiFi error");
            break;
        }

        return false;
    }
}

bool WiFiManager::isConnected()
{
    return WiFi.status() == WL_CONNECTED;
}