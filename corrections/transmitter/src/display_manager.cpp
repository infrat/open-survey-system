#include "display_manager.h"
#include "oss_splash.h"

DisplayManager::DisplayManager() : display(nullptr)
{
}

bool DisplayManager::begin()
{
    display = new SSD1306Wire(DISPLAY_ADDRESS, SDA_PIN, SCL_PIN);

    if (!display->init())
    {
        Serial.println("Display initialization failed!");
        return false;
    }

    display->flipScreenVertically();
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_LEFT);

    Serial.println("Display initialized successfully");
    return true;
}

void DisplayManager::showStatus(const String &title, const String &message)
{
    if (!display)
        return;

    clear();
    display->setFont(ArialMT_Plain_10);

    // Top line: Header
    display->drawString(0, 0, "OSS RTCM Transmitter");

    // Underline
    display->drawLine(0, 12, 128, 12);

    // Status message
    display->drawString(0, 20, title + ": " + message);

    display->display();
}

void DisplayManager::showError(const String &message)
{
    if (!display)
        return;

    clear();
    display->setFont(ArialMT_Plain_10);

    // Top line: Header
    display->drawString(0, 0, "OSS RTCM Transmitter");

    // Underline
    display->drawLine(0, 12, 128, 12);

    // Error message
    display->drawString(0, 20, "ERROR: " + message);
    display->drawString(0, 32, "Check serial monitor");

    display->display();
}

void DisplayManager::showStats(unsigned long received, unsigned long transmitted)
{
    if (!display)
        return;

    clear();
    display->setFont(ArialMT_Plain_10);

    // Top line: Header
    display->drawString(0, 0, "OSS RTCM Transmitter");

    // Underline
    display->drawLine(0, 12, 128, 12);

    // RX and TX counters
    display->drawString(0, 20, "RX: " + String(received));
    display->drawString(0, 32, "TX: " + String(transmitted));

    display->display();
}

void DisplayManager::showSplashScreen()
{
    display->clear();

    // Draw bitmap full screen (128x64 bitmap on 128x64 screen)
    display->drawXbm(0, 0, splash_width, splash_height, splashBitmap);

    display->display();
}

void DisplayManager::updateStatsDisplay(SystemState state, uint32_t rxMsgPerSec, uint32_t txMsgPerSec,
                                        bool wifiConnected, bool ntripConnected)
{
    if (!display)
        return;

    clear();
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    // Title
    display->setFont(ArialMT_Plain_10);
    // Top line: Header
    display->drawString(0, 0, "OSS RTCM Transmitter");

    // Underline
    display->drawLine(0, 12, 128, 12);

    // Status line
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    const char *stateStr = "UNKNOWN";
    if (state == STATE_RUNNING)
        stateStr = "RUNNING";
    else if (state == STATE_MAINTENANCE_TX)
        stateStr = "MAINT TX";
    else if (state == STATE_MAINTENANCE_RX)
        stateStr = "MAINT RX";
    else if (state == STATE_STANDBY)
        stateStr = "STANDBY";
    else if (state == STATE_ERROR)
        stateStr = "ERROR";

    display->drawString(0, 15, "Status:");
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(128, 15, stateStr);

    // RTCM RX (incoming from NTRIP)
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 28, "RX:");
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    char rxRate[16];
    snprintf(rxRate, sizeof(rxRate), "%u msg/s", rxMsgPerSec);
    display->drawString(128, 28, rxRate);

    // RTCM TX (outgoing to LoRa)
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 38, "TX:");
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    char txRate[16];
    snprintf(txRate, sizeof(txRate), "%u msg/s", txMsgPerSec);
    display->drawString(128, 38, txRate);

    // WiFi status indicator
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 51, "WiFi:");
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(50, 51, wifiConnected ? "OK" : "ERR");

    // NTRIP status indicator
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(60, 51, "NTRIP:");
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(128, 51, ntripConnected ? "OK" : "ERR");

    display->display();
}

void DisplayManager::clear()
{
    if (!display)
        return;
    display->clear();
}