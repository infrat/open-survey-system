#include "bridge.h"
#include "logger.h"

Bridge *Bridge::_instance = nullptr;

void Bridge::begin(UARTHandler *uart, ITransport *transport)
{
    _uart = uart;
    _transport = transport;
    _instance = this;

    _transport->setDataCallback(&Bridge::onTransportData);

    logPrintln("[Bridge] GPS (UART) → %s → client", _transport->name());
    logPrintln("[Bridge] client → %s → GPS (UART)", _transport->name());
}

// Invoked from the transport when data arrives from the client (client → UART)
void Bridge::onTransportData(const uint8_t *data, size_t length)
{
    if (!_instance)
    {
        return;
    }
#if BRIDGE_LOG_TRAFFIC
    logPrint("[Bridge] %s → UART: %d bytes\n", _instance->_transport->name(), (int)length);
#endif
    _instance->_uart->writeData(data, length);
}

void Bridge::loop()
{
    _transport->loop();

    // ===== UART → transport (GPS → client) =====
    // Check if there's GPS data and send IMMEDIATELY
    size_t availableBytes = _uart->available();
    if (availableBytes > 0)
    {
        // Read as much as available (don't wait for full buffer)
        size_t bytesToRead = min(availableBytes, (size_t)(UART_BUF_SIZE - 1));
        int bytesRead = _uart->readData(_buffer, bytesToRead);

        if (bytesRead > 0)
        {
#if BRIDGE_LOG_TRAFFIC
            logPrint("[Bridge] UART → %s: %d bytes\n", _transport->name(), bytesRead);
#endif
            // BLE chunking / SPP queueing / TCP fan-out is inside sendData
            if (_transport->isConnected())
            {
                _transport->sendData(_buffer, bytesRead);
            }
        }
    }

    updateLed();
}

void Bridge::updateLed()
{
    if (_transport->isConnected())
    {
        // Client connected - LED solid (HIGH = ON)
        digitalWrite(LINK_STATUS_LED_PIN, HIGH);
    }
    else if (millis() - _lastBlink >= LINK_LED_BLINK_MS)
    {
        // No connection - LED blinks every 250ms
        _ledState = !_ledState;
        digitalWrite(LINK_STATUS_LED_PIN, _ledState ? HIGH : LOW);
        _lastBlink = millis();
    }
}
