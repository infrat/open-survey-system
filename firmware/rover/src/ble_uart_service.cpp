#include "config.h"

#if ROVER_TRANSPORT == TRANSPORT_BLE

#include <esp_bt.h>
#include "ble_uart_service.h"

BLEUARTService::BLEUARTService()
    : _pServer(nullptr), _pService(nullptr), _pTxCharacteristic(nullptr), _pRxCharacteristic(nullptr), _dataCallback(nullptr), _logCallback(nullptr), _deviceConnected(false)
{
}

bool BLEUARTService::begin(const char *deviceName)
{
    if (_logCallback)
        _logCallback("[BLE] Initializing BLE...");

    // Initialize NimBLE
    NimBLEDevice::init(deviceName);

    // Set MTU
    NimBLEDevice::setMTU(BLE_MTU_SIZE);

    // Create BLE Server
    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    // Create Nordic UART Service
    _pService = _pServer->createService(SERVICE_UUID);

    // TX Characteristic (ESP32 → iOS) - Notify
    _pTxCharacteristic = _pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY);

    // RX Characteristic (iOS → ESP32) - Write/Write Without Response
    _pRxCharacteristic = _pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);

    _pRxCharacteristic->setCallbacks(new RxCallbacks(this));

    // Start service
    _pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // helper functions for connection parameters
    pAdvertising->setMaxPreferred(0x12);

    NimBLEDevice::startAdvertising();

    if (_logCallback)
    {
        _logCallback("[BLE] Nordic UART Service started");
        _logCallback("[BLE] Device name: %s", deviceName);
        _logCallback("[BLE] Waiting for iOS connection...");
    }

    return true;
}

void BLEUARTService::setTxPower()
{
    // Low power mode - range drops to ~10-15m, which is enough rover → phone
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, BLE_TX_POWER);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, BLE_TX_POWER);
    esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, BLE_TX_POWER);

    if (_logCallback)
        _logCallback("[BLE] TX Power set to low power mode (BLE_TX_POWER=%d)", (int)BLE_TX_POWER);
}

void BLEUARTService::sendData(const uint8_t *data, size_t length)
{
    if (_deviceConnected && _pTxCharacteristic && length > 0)
    {
        // If data is large, split into chunks
        size_t offset = 0;

        while (offset < length)
        {
            size_t chunkSize = min((size_t)BLE_CHUNK_SIZE, length - offset);

            // Send data chunk via BLE notification
            _pTxCharacteristic->setValue(data + offset, chunkSize);
            _pTxCharacteristic->notify();

            offset += chunkSize;

            // Small delay between chunks for stability
            if (offset < length)
            {
                delay(10);
            }
        }
    }
}

void BLEUARTService::sendString(const char *str)
{
    if (str)
    {
        sendData((const uint8_t *)str, strlen(str));
    }
}

void BLEUARTService::setDataCallback(TransportDataCallback callback)
{
    _dataCallback = callback;
}

void BLEUARTService::setLogCallback(TransportLogCallback callback)
{
    _logCallback = callback;
}

bool BLEUARTService::isConnected()
{
    return _deviceConnected;
}

uint32_t BLEUARTService::getConnectedCount()
{
    if (_pServer)
    {
        return _pServer->getConnectedCount();
    }
    return 0;
}

// ===== Server Callbacks =====

void BLEUARTService::ServerCallbacks::onConnect(BLEServer *pServer)
{
    _service->_deviceConnected = true;
    if (_service->_logCallback)
    {
        _service->_logCallback("[BLE] iOS device connected!");
    }

    // Optional: update connection parameters for better performance
    // NimBLEDevice::updateConnParams(...)
}

void BLEUARTService::ServerCallbacks::onDisconnect(BLEServer *pServer)
{
    _service->_deviceConnected = false;
    if (_service->_logCallback)
    {
        _service->_logCallback("[BLE] iOS device disconnected");
    }

    // Restart advertising
    delay(500);
    NimBLEDevice::startAdvertising();
    if (_service->_logCallback)
    {
        _service->_logCallback("[BLE] Restarted advertising");
    }
}

// ===== RX Characteristic Callbacks =====

void BLEUARTService::RxCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    // Receive data from iOS
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {

        // Call callback if set
        if (_service->_dataCallback)
        {
            _service->_dataCallback((const uint8_t *)value.data(), value.length());
        }
    }
}

#endif // ROVER_TRANSPORT == TRANSPORT_BLE
