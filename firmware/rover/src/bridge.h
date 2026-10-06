#ifndef BRIDGE_H
#define BRIDGE_H

#include <Arduino.h>
#include "config.h"
#include "transport.h"
#include "uart_handler.h"

/**
 * @brief The actual product: GNSS UART ↔ transport, plus the link status LED.
 *
 * GNSS → client: NMEA read from the UART is pushed out immediately.
 * client → GNSS: RTCM arriving on the transport is written to the UART.
 */
class Bridge
{
public:
    void begin(UARTHandler *uart, ITransport *transport);
    void loop();

private:
    static void onTransportData(const uint8_t *data, size_t length);
    void updateLed();

    UARTHandler *_uart = nullptr;
    ITransport *_transport = nullptr;
    uint8_t _buffer[UART_BUF_SIZE];
    unsigned long _lastBlink = 0;
    bool _ledState = false;

    static Bridge *_instance;
};

#endif // BRIDGE_H
