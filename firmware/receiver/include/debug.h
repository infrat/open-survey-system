#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>
#include "config.h"

// Thread-safe debug mutex (defined in debug.cpp)
extern SemaphoreHandle_t debugMutex;

// Debug macros - thread-safe for dual-core using FreeRTOS mutex
// Mutex doesn't disable interrupts, avoiding watchdog timeouts
#if ENABLE_UART_LOGGING
#define DEBUG_PRINT(x)                                 \
    do                                                 \
    {                                                  \
        if (debugMutex)                                \
            xSemaphoreTake(debugMutex, portMAX_DELAY); \
        Serial.print(x);                               \
        if (debugMutex)                                \
            xSemaphoreGive(debugMutex);                \
    } while (0)
#define DEBUG_PRINT_HEX(x)                             \
    do                                                 \
    {                                                  \
        if (debugMutex)                                \
            xSemaphoreTake(debugMutex, portMAX_DELAY); \
        Serial.print(x, HEX);                          \
        if (debugMutex)                                \
            xSemaphoreGive(debugMutex);                \
    } while (0)
#define DEBUG_PRINTLN(x)                               \
    do                                                 \
    {                                                  \
        if (debugMutex)                                \
            xSemaphoreTake(debugMutex, portMAX_DELAY); \
        Serial.println(x);                             \
        if (debugMutex)                                \
            xSemaphoreGive(debugMutex);                \
    } while (0)
#define DEBUG_PRINTF(format, ...)                      \
    do                                                 \
    {                                                  \
        if (debugMutex)                                \
            xSemaphoreTake(debugMutex, portMAX_DELAY); \
        Serial.printf(format, ##__VA_ARGS__);          \
        if (debugMutex)                                \
            xSemaphoreGive(debugMutex);                \
    } while (0)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINT_HEX(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTF(format, ...)
#endif

// Debug functions
void initDebug();

#endif // DEBUG_H