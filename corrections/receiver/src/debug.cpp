#include "debug.h"
#include <Arduino.h>

SemaphoreHandle_t debugMutex = NULL;

void initDebug()
{
    debugMutex = xSemaphoreCreateMutex();
#if ENABLE_UART_LOGGING
    Serial.begin(115200);
    delay(1000);
#endif
}

void logHexData(const uint8_t *data, size_t length)
{
    DEBUG_PRINT("HEX: ");
    for (size_t i = 0; i < length; i++)
    {
        if (data[i] < 0x10)
            DEBUG_PRINT("0");
        DEBUG_PRINT_HEX(data[i]);
        if (i < length - 1)
            DEBUG_PRINT(" ");
    }
    DEBUG_PRINTLN("");
}