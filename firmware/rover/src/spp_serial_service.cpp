#include "config.h"
#include "spp_serial_service.h"

SPPSerialService *SPPSerialService::_instance = nullptr;

SPPSerialService::SPPSerialService()
    : _dataCallback(nullptr), _logCallback(nullptr)
{
    _instance = this;
}

bool SPPSerialService::begin(const char *deviceName)
{
    if (_logCallback)
        _logCallback("[SPP] Initializing Bluetooth Classic...");

    // Push incoming payloads straight to the bridge callback. BluetoothSerial
    // bypasses its internal RX queue entirely once a data callback is set,
    // so nothing is buffered twice and read()/available() stay unused.
    _serial.onData([this](const uint8_t *buffer, size_t size)
                   {
        if (_dataCallback && size > 0)
        {
            _dataCallback(buffer, size);
        } });

    // Connect/disconnect edges for logging and the status LED
    _serial.register_callback(&SPPSerialService::sppEventHandler);

    // Mirror of BLEUARTService::begin(): this boot only uses Classic BT, so
    // release the BLE half of the dual-mode controller and enable it
    // Classic-only before BluetoothSerial gets to it.
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
    esp_bt_controller_config_t btCfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    btCfg.mode = ESP_BT_MODE_CLASSIC_BT;
    esp_err_t err = esp_bt_controller_init(&btCfg);
    if (err == ESP_OK)
    {
        err = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
    }
    if (err != ESP_OK)
    {
        if (_logCallback)
            _logCallback("[SPP] Classic-only controller start failed [0x%X]", err);
        return false;
    }

    if (!_serial.begin(String(deviceName)))
    {
        if (_logCallback)
            _logCallback("[SPP] begin() failed - is Bluedroid/Classic BT enabled?");
        return false;
    }

    if (_logCallback)
    {
        _logCallback("[SPP] Serial Port Profile server started");
        _logCallback("[SPP] Device name: %s", deviceName);
        _logCallback("[SPP] Pair from Android and open the serial port...");
    }

    return true;
}

void SPPSerialService::setTxPower()
{
    // Low power mode - min and max are pinned to the same level so the
    // controller cannot ramp up on its own.
    esp_err_t err = esp_bredr_tx_power_set(SPP_TX_POWER, SPP_TX_POWER);

    if (_logCallback)
    {
        if (err == ESP_OK)
        {
            _logCallback("[SPP] TX Power set to low power mode (SPP_TX_POWER=%d)", (int)SPP_TX_POWER);
        }
        else
        {
            _logCallback("[SPP] TX Power set failed [0x%X] - using controller default", err);
        }
    }
}

void SPPSerialService::sendData(const uint8_t *data, size_t length)
{
    if (length == 0 || !_serial.hasClient())
    {
        return;
    }

    // SPP is a stream, not a datagram service: write() copies the payload to
    // the TX queue and the BluetoothSerial task handles fragmentation. No
    // BLE-style chunking or inter-chunk delay required here.
    _serial.write(data, length);
}

void SPPSerialService::sendString(const char *str)
{
    if (str)
    {
        sendData((const uint8_t *)str, strlen(str));
    }
}

void SPPSerialService::setDataCallback(TransportDataCallback callback)
{
    _dataCallback = callback;
}

void SPPSerialService::setLogCallback(TransportLogCallback callback)
{
    _logCallback = callback;
}

bool SPPSerialService::isConnected()
{
    return _serial.hasClient();
}

uint32_t SPPSerialService::getConnectedCount()
{
    return _serial.hasClient() ? 1 : 0;
}

// ===== SPP Event Callback =====

void SPPSerialService::sppEventHandler(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    if (!_instance || !_instance->_logCallback)
    {
        return;
    }

    // Invoked from the Bluedroid task after BluetoothSerial has already
    // updated its own state, so hasClient() is consistent here.
    switch (event)
    {
    case ESP_SPP_SRV_OPEN_EVT:
        _instance->_logCallback("[SPP] Client connected!");
        break;

    case ESP_SPP_CLOSE_EVT:
        _instance->_logCallback("[SPP] Client disconnected");
        break;

    default:
        break;
    }
}

