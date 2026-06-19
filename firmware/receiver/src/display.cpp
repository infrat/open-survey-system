#include "display.h"
#include "debug.h"
#include "config.h"
#include "wifi_manager.h"
#include "ntrip_server.h"
#include "lora_radio.h"
#include "oss_splash.h"
#include <Wire.h>

// Display Manager Class
class DisplayManager
{
private:
    SSD1306Wire *display;

public:
    DisplayManager() : display(nullptr) {}

    bool begin()
    {
        display = new SSD1306Wire(0x3c, OLED_SDA, OLED_SCL);

        DEBUG_PRINTLN("Initializing OLED...");
        DEBUG_PRINTF("OLED pins - SDA: %d, SCL: %d, RST: %d\n", OLED_SDA, OLED_SCL, OLED_RST);

        // Initialize I2C with proper pins
        Wire.begin(OLED_SDA, OLED_SCL);
        Wire.setClock(100000); // Set I2C clock to 100kHz for stability
        delay(10);

        if (!display->init())
        {
            DEBUG_PRINTLN("OLED initialization failed!");
            return false;
        }

        DEBUG_PRINTLN("OLED initialized successfully!");
        display->flipScreenVertically();
        display->setFont(ArialMT_Plain_10);

        return true;
    }

    void showSplashScreen()
    {
        display->clear();

        // Draw bitmap full screen (128x64 bitmap on 128x64 screen)
        display->drawXbm(0, 0, splash_width, splash_height, splashBitmap);

        display->display();
        DEBUG_PRINTLN("Splash screen displayed");
    }

    void showStats()
    {
        if (!display)
            return;

        clear();
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        // Header with underline
        display->drawString(0, 0, "OSS RTCM Receiver");
        display->drawLine(0, 12, 128, 12);

        // Line 1: RTCM RX rate + age
        float msgRate = getMessageRate();
        uint32_t secondsSince = getSecondsSinceLastMessage();
        display->drawString(0, 13, "RX: ");
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 13, String(msgRate, 1) + "/s (" + String(secondsSince) + "s)");

        // Line 2: Error rate
        float errorRate = getErrorRate();
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 26, "Err: ");
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 26, String(errorRate, 1) + "%");

        // Line 3: WiFi clients
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 39, "WiFi: ");
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 39, String(getConnectedClients()) + "/" + String(WIFI_MAX_CONNECTIONS));

        // Line 4: NTRIP clients
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 52, "NTRIP: ");
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 52, String(getNTRIPClientCount()) + "/" + String(NTRIP_MAX_CLIENTS));

        display->display();
    }

    void showError(const String &message)
    {
        if (!display)
            return;

        clear();
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_LEFT);

        // Header with underline
        display->drawString(0, 0, "LoRa RTCM Gateway");
        display->drawLine(0, 12, 128, 12);

        // Error message
        display->drawString(0, 20, "ERROR:");
        display->drawString(0, 32, message);
        display->drawString(0, 44, "Check serial monitor");

        display->display();
    }

    void clear()
    {
        if (!display)
            return;
        display->clear();
    }
};

// Global display manager instance
DisplayManager displayManager;

// Public interface functions
void setupOLED()
{
    if (!displayManager.begin())
    {
        DEBUG_PRINTLN("Display initialization failed!");
        return;
    }
}

void showSplashScreen()
{
    displayManager.showSplashScreen();
}

void updateDisplay()
{
    displayManager.showStats();
}

void showError(const char *message)
{
    displayManager.showError(String(message));
}