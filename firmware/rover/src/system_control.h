#ifndef SYSTEM_CONTROL_H
#define SYSTEM_CONTROL_H

#include <Arduino.h>

// Restart from the main loop after a short delay, so the HTTP/Telnet reply that
// triggered it can still go out. Never block or restart inside a handler.
void scheduleRestart(uint32_t delayMs);

// Call from loop()
void systemControlLoop();

#endif // SYSTEM_CONTROL_H
