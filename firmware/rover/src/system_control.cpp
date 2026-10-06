#include "system_control.h"
#include "logger.h"

static volatile bool g_restartPending = false;
static volatile uint32_t g_restartAt = 0;

void scheduleRestart(uint32_t delayMs)
{
    g_restartAt = millis() + delayMs;
    g_restartPending = true;
}

void systemControlLoop()
{
    if (g_restartPending && (int32_t)(millis() - g_restartAt) >= 0)
    {
        logPrintln("[System] Restarting...");
        Serial.flush();
        ESP.restart();
    }
}
