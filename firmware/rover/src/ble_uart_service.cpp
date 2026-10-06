#include "config.h"
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

    // The Arduino core always brings the controller up in dual mode (BLE +
    // Classic). This boot only uses BLE, so hand the Classic half back to the
    // heap and enable the controller BLE-only before BLEDevice::init() gets to
    // it (btStart() leaves an already enabled controller alone).
    esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    esp_bt_controller_config_t btCfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    btCfg.mode = ESP_BT_MODE_BLE;
    esp_err_t err = esp_bt_controller_init(&btCfg);
    if (err == ESP_OK)
    {
        err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    }
    if (err != ESP_OK)
    {
        if (_logCallback)
            _logCallback("[BLE] BLE-only controller start failed [0x%X]", err);
        return false;
    }

    // Initialize the BLE host
    BLEDevice::init(deviceName);

    // Set MTU
    BLEDevice::setMTU(BLE_MTU_SIZE);

    // Create BLE Server
    _pServer = BLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks(this));

    // Create Nordic UART Service
    _pService = _pServer->createService(SERVICE_UUID);

    // TX Characteristic (ESP32 → mobile) - Notify. Bluedroid does not add the
    // client configuration descriptor on its own, apps need it to subscribe.
    _pTxCharacteristic = _pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY);
    _pTxCharacteristic->addDescriptor(new BLE2902());

    // RX Characteristic (mobile → ESP32) - Write/Write Without Response
    _pRxCharacteristic = _pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);

    _pRxCharacteristic->setCallbacks(new RxCallbacks(this));

    // Start service
    _pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // helper functions for connection parameters
    pAdvertising->setMaxPreferred(0x12);

    BLEDevice::startAdvertising();

    if (_logCallback)
    {
        _logCallback("[BLE] Nordic UART Service started");
        _logCallback("[BLE] Device name: %s", deviceName);
        _logCallback("[BLE] Waiting for connection...");
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
            _pTxCharacteristic->setValue((uint8_t *)(data + offset), chunkSize);
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
        _service->_logCallback("[BLE] Device connected!");
    }
}

void BLEUARTService::ServerCallbacks::onDisconnect(BLEServer *pServer)
{
    _service->_deviceConnected = false;
    if (_service->_logCallback)
    {
        _service->_logCallback("[BLE] Device disconnected");
    }

    // Restart advertising
    delay(500);
    BLEDevice::startAdvertising();
    if (_service->_logCallback)
    {
        _service->_logCallback("[BLE] Restarted advertising");
    }
}

// ===== RX Characteristic Callbacks =====

void BLEUARTService::RxCallbacks::onWrite(BLECharacteristic *pCharacteristic)
{
    // Receive data from the mobile app
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
