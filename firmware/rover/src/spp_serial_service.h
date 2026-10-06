#ifndef SPP_SERIAL_SERVICE_H
#define SPP_SERIAL_SERVICE_H

#include "config.h"

#if ROVER_TRANSPORT == TRANSPORT_SPP

#include <BluetoothSerial.h>
#include <esp_bt.h>
#include "transport.h"

/**
 * @brief Bluetooth Classic (SPP) side of the UART bridge.
 *
 * Drop-in alternative to BLEUARTService for Android clients. SPP is a plain
 * byte stream: there is no MTU to respect and no chunking needed - the
 * BluetoothSerial TX task buffers and fragments outgoing data itself.
 *
 * @warning iOS cannot use SPP without MFi certification. Build with
 *          ROVER_TRANSPORT=TRANSPORT_BLE for iOS support.
 */
class SPPSerialService : public ITransport
{
public:
    SPPSerialService();

    // Start the SPP server and become discoverable/pairable
    bool begin(const char *deviceName) override;

    // Apply low BR/EDR TX power (see SPP_TX_POWER)
    void setTxPower() override;

    // Send data to the paired phone (ESP32 → app)
    void sendData(const uint8_t *data, size_t length) override;
    void sendString(const char *str);

    // Set callback for incoming SPP data (app → ESP32)
    void setDataCallback(TransportDataCallback callback) override;

    // Set callback for logging
    void setLogCallback(TransportLogCallback callback) override;

    // Is an SPP client connected?
    bool isConnected() override;

    // SPP server accepts a single client, so this is 0 or 1
    uint32_t getConnectedCount() override;

    const char *name() const override { return "SPP"; }

private:
    // BluetoothSerial only exposes a polled hasClient(); the raw SPP events
    // give us proper connect/disconnect edges for logging. The ESP-IDF
    // callback is a plain C function pointer, hence the static trampoline.
    static void sppEventHandler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param);
    static SPPSerialService *_instance;

    BluetoothSerial _serial;
    TransportDataCallback _dataCallback;
    TransportLogCallback _logCallback;
};

#endif // ROVER_TRANSPORT == TRANSPORT_SPP
#endif // SPP_SERIAL_SERVICE_H
