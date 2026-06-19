#include "uart_handler.h"

UARTHandler::UARTHandler()
    : _logCallback(nullptr)
{
    // Use HardwareSerial for UART1
    _serial = &Serial1;
}

bool UARTHandler::begin()
{
    // Initialize UART1 with GPS configuration
    _serial->begin(GPS_BAUD_RATE, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    // Set RX/TX buffer size
    _serial->setRxBufferSize(UART_BUF_SIZE);

    if (_logCallback)
    {
        _logCallback("[UART] Initialized GPS UART");
        _logCallback("[UART] RX: GPIO%d, TX: GPIO%d, Baud: %d",
                     GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD_RATE);
    }

    return true;
}

void UARTHandler::setLogCallback(UARTLogCallback callback)
{
    _logCallback = callback;
}

int UARTHandler::available()
{
    return _serial->available();
}

int UARTHandler::readData(uint8_t *buffer, size_t maxLen)
{
    if (!_serial->available())
    {
        return 0;
    }

    // Read available data (max maxLen bytes)
    int bytesRead = _serial->readBytes(buffer, maxLen);

    return bytesRead;
}

void UARTHandler::writeData(const uint8_t *data, size_t length)
{
    if (length > 0)
    {
        _serial->write(data, length);
        _serial->flush(); // Wait until data is sent
    }
}

void UARTHandler::writeString(const char *str)
{
    _serial->print(str);
    _serial->flush();
}
