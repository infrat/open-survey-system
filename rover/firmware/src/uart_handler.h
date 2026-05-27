#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <Arduino.h>
#include "config.h"

// Callback for logging (to both Serial and Telnet)
typedef void (*UARTLogCallback)(const char *format, ...);

class UARTHandler
{
public:
    UARTHandler();

    // Initialize UART
    bool begin();

    // Set callback for logging
    void setLogCallback(UARTLogCallback callback);

    // Check if data is available
    int available();

    // Read data from UART (returns number of bytes read)
    int readData(uint8_t *buffer, size_t maxLen);

    // Send data via UART
    void writeData(const uint8_t *data, size_t length);

    // Send string via UART
    void writeString(const char *str);

private:
    HardwareSerial *_serial;
    UARTLogCallback _logCallback;
};

#endif // UART_HANDLER_H
