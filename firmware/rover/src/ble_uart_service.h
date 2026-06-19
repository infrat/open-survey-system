#ifndef BLE_UART_SERVICE_H
#define BLE_UART_SERVICE_H

#include <NimBLEDevice.h>
#include "config.h"

// Callback for logging (to both Serial and Telnet)
typedef void (*BLELogCallback)(const char *format, ...);

// Callback for receiving data from BLE (iOS → ESP32)
typedef void (*BLEDataCallback)(const uint8_t *data, size_t length);

class BLEUARTService
{
public:
    BLEUARTService();

    // Initialize BLE and Nordic UART Service
    bool begin(const char *deviceName);

    // Send data via BLE (ESP32 → iOS)
    void sendData(const uint8_t *data, size_t length);
    void sendString(const char *str);

    // Set callback for incoming BLE data
    void setDataCallback(BLEDataCallback callback);

    // Set callback for logging
    void setLogCallback(BLELogCallback callback);

    // Check if device is connected
    bool isConnected();

    // Get number of connected devices
    uint32_t getConnectedCount();

private:
    BLEServer *_pServer;
    BLEService *_pService;
    BLECharacteristic *_pTxCharacteristic; // ESP32 → iOS (Notify)
    BLECharacteristic *_pRxCharacteristic; // iOS → ESP32 (Write)

    BLEDataCallback _dataCallback;
    BLELogCallback _logCallback;
    bool _deviceConnected;

    // Callback class for connections
    class ServerCallbacks : public BLEServerCallbacks
    {
    public:
        ServerCallbacks(BLEUARTService *service) : _service(service) {}

        void onConnect(BLEServer *pServer);
        void onDisconnect(BLEServer *pServer);

    private:
        BLEUARTService *_service;
    };

    // Callback class for RX characteristic (receiving data)
    class RxCallbacks : public BLECharacteristicCallbacks
    {
    public:
        RxCallbacks(BLEUARTService *service) : _service(service) {}

        void onWrite(BLECharacteristic *pCharacteristic);

    private:
        BLEUARTService *_service;
    };

    friend class ServerCallbacks;
    friend class RxCallbacks;
};

#endif // BLE_UART_SERVICE_H
