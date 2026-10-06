#ifndef TRANSPORT_H
#define TRANSPORT_H

#include "config.h"
#include <Arduino.h>

// Callback for logging (to both Serial and Telnet)
typedef void (*TransportLogCallback)(const char *format, ...);

// Callback for data arriving from the mobile app (app → ESP32 → GNSS)
typedef void (*TransportDataCallback)(const uint8_t *data, size_t length);

/**
 * @brief Radio-agnostic contract for the client-facing side of the bridge.
 *
 * The bridge talks only to this interface. The implementation (BLE NUS,
 * Bluetooth SPP or WiFi TCP) is picked once at boot by transport_factory.
 */
class ITransport
{
public:
    virtual ~ITransport() {}

    // Bring the link up and start advertising / listening
    virtual bool begin(const char *deviceName) = 0;

    // Apply the low-power TX level from config.h. Call AFTER begin().
    virtual void setTxPower() = 0;

    // Send data to the client (GNSS → app)
    virtual void sendData(const uint8_t *data, size_t length) = 0;

    // Set callback for incoming data (app → GNSS)
    virtual void setDataCallback(TransportDataCallback callback) = 0;

    // Set callback for logging
    virtual void setLogCallback(TransportLogCallback callback) = 0;

    // Is a client currently connected?
    virtual bool isConnected() = 0;

    // Number of connected clients
    virtual uint32_t getConnectedCount() = 0;

    // Short label for logs, e.g. "BLE", "SPP" or "TCP"
    virtual const char *name() const = 0;

    // Polled from the main loop. Only transports that are not callback driven
    // (TCP) need it.
    virtual void loop() {}
};

#endif // TRANSPORT_H
