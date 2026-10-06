#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "config.h"
#include <Arduino.h>

#if !defined(ROVER_TRANSPORT) || !defined(TRANSPORT_BLE) || \
    !defined(TRANSPORT_SPP)
#error "ROVER_TRANSPORT / TRANSPORT_* missing - config.h must be included first"
#endif

// Callback for logging (to both Serial and Telnet)
typedef void (*TransportLogCallback)(const char *format, ...);

// Callback for data arriving from the mobile app (app → ESP32 → GNSS)
typedef void (*TransportDataCallback)(const uint8_t *data, size_t length);

/**
 * @brief Radio-agnostic contract for the mobile-facing side of the bridge.
 *
 * main.cpp talks only to this interface, so the BLE (NimBLE NUS) and
 * Bluetooth Classic (SPP) implementations are interchangeable at compile time.
 */
class ITransport
{
public:
    virtual ~ITransport() {}

    // Bring the radio up and start advertising / become discoverable
    virtual bool begin(const char *deviceName) = 0;

    // Apply the low-power TX level from config.h. Call AFTER begin().
    virtual void setTxPower() = 0;

    // Send data to the mobile app (GNSS → app)
    virtual void sendData(const uint8_t *data, size_t length) = 0;

    // Set callback for incoming data (app → GNSS)
    virtual void setDataCallback(TransportDataCallback callback) = 0;

    // Set callback for logging
    virtual void setLogCallback(TransportLogCallback callback) = 0;

    // Is a mobile app currently connected?
    virtual bool isConnected() = 0;

    // Number of connected clients
    virtual uint32_t getConnectedCount() = 0;

    // Short label for logs, e.g. "BLE" or "SPP"
    virtual const char *name() const = 0;
};

// The concrete implementation is picked in rover_transport.h - this header
// deliberately stays free of any Bluetooth stack include so both
// implementations can include it.

#endif // TRANSPORT_H
