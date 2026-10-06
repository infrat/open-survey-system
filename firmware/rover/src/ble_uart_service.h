#ifndef BLE_UART_SERVICE_H
#define BLE_UART_SERVICE_H

#include "config.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "transport.h"

/**
 * @brief BLE Nordic UART Service (NUS) side of the UART bridge.
 *
 * Runs on the Bluedroid stack (the same one SPP needs), so a single firmware
 * image can offer either radio. Only one of them is brought up per boot.
 */
class BLEUARTService : public ITransport
{
public:
    BLEUARTService();

    // Initialize BLE and Nordic UART Service
    bool begin(const char *deviceName) override;

    // Apply low BLE TX power (see BLE_TX_POWER)
    void setTxPower() override;

    // Send data via BLE (ESP32 → mobile app)
    void sendData(const uint8_t *data, size_t length) override;
    void sendString(const char *str);

    // Set callback for incoming BLE data
    void setDataCallback(TransportDataCallback callback) override;

    // Set callback for logging
    void setLogCallback(TransportLogCallback callback) override;

    // Check if device is connected
    bool isConnected() override;

    // Get number of connected devices
    uint32_t getConnectedCount() override;

    const char *name() const override { return "BLE"; }

private:
    BLEServer *_pServer;
    BLEService *_pService;
    BLECharacteristic *_pTxCharacteristic; // ESP32 → mobile (Notify)
    BLECharacteristic *_pRxCharacteristic; // mobile → ESP32 (Write)

    TransportDataCallback _dataCallback;
    TransportLogCallback _logCallback;
    volatile bool _deviceConnected;

    // Callback class for connections
    class ServerCallbacks : public BLEServerCallbacks
    {
    public:
        ServerCallbacks(BLEUARTService *service)
            : _service(service) {}

        void onConnect(BLEServer *pServer) override;
        void onDisconnect(BLEServer *pServer) override;

    private:
        BLEUARTService *_service;
    };

    // Callback class for RX characteristic (receiving data)
    class RxCallbacks : public BLECharacteristicCallbacks
    {
    public:
        RxCallbacks(BLEUARTService *service)
            : _service(service) {}

        void onWrite(BLECharacteristic *pCharacteristic) override;

    private:
        BLEUARTService *_service;
    };

    friend class ServerCallbacks;
    friend class RxCallbacks;
};

#endif // BLE_UART_SERVICE_H
