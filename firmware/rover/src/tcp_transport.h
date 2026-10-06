#ifndef TCP_TRANSPORT_H
#define TCP_TRANSPORT_H

#include "config.h"
#include <WiFi.h>
#include "transport.h"

/**
 * @brief Raw TCP socket transport (NMEA out, RTCM in) for apps and tools that
 *        connect over WiFi, e.g. a survey app pointed at ossrtk.local:10110.
 *
 * No Bluetooth stack is started in this mode, its RAM is handed back to the
 * heap. Up to TCP_MAX_CLIENTS clients can listen at once; bytes written by any
 * of them go to the GNSS receiver.
 */
class TcpTransport : public ITransport
{
public:
    explicit TcpTransport(uint16_t port);

    bool begin(const char *deviceName) override;
    void setTxPower() override {} // WiFi power is managed by WifiManager
    void sendData(const uint8_t *data, size_t length) override;
    void setDataCallback(TransportDataCallback callback) override { _dataCallback = callback; }
    void setLogCallback(TransportLogCallback callback) override { _logCallback = callback; }
    bool isConnected() override;
    uint32_t getConnectedCount() override;
    const char *name() const override { return "TCP"; }
    void loop() override;

private:
    uint16_t _port;
    WiFiServer _server;
    WiFiClient _clients[TCP_MAX_CLIENTS];
    TransportDataCallback _dataCallback;
    TransportLogCallback _logCallback;
};

#endif // TCP_TRANSPORT_H
