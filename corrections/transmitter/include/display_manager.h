#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <Arduino.h>
#include <SSD1306Wire.h>
#include "system_state.h"

class DisplayManager
{
public:
    DisplayManager();
    bool begin();
    void showStatus(const String &title, const String &message);
    void showError(const String &message);
    void showStats(unsigned long received, unsigned long transmitted); // Legacy method
    void updateStatsDisplay(SystemState state, uint32_t rxMsgPerSec, uint32_t txMsgPerSec,
                            bool wifiConnected, bool ntripConnected);
    void showSplashScreen();
    void clear();

private:
    SSD1306Wire *display;
    static const int SDA_PIN = 21;
    static const int SCL_PIN = 22;
    static const int DISPLAY_ADDRESS = 0x3c;
    static const int DISPLAY_WIDTH = 128;
    static const int DISPLAY_HEIGHT = 64;
};

#endif