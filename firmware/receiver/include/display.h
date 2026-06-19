#ifndef DISPLAY_H
#define DISPLAY_H

#include "SSD1306Wire.h"

// Display manager functions
void setupOLED();
void showSplashScreen();
void updateDisplay();
void showError(const char *message);

#endif // DISPLAY_H