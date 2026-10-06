#include "tcp_transport.h"
#include <esp_bt.h>

TcpTransport::TcpTransport(uint16_t port)
    : _port(port), _server(port), _dataCallback(nullptr), _logCallback(nullptr)
{
}

bool TcpTransport::begin(const char *deviceName)
{
    (void)deviceName;

    // Bluetooth is never started in this mode, give its memory back. Fails
    // harmlessly if it was already released.
    esp_bt_mem_release(ESP_BT_MODE_BTDM);

    _server.begin();
    _server.setNoDelay(true);

    if (_logCallback)
    {
        _logCallback("[TCP] Listening on port %u (max %d clients)", _port, TCP_MAX_CLIENTS);
    }
    return true;
}

void TcpTransport::loop()
{
    // Accept new clients into a free slot
    if (_server.hasClient())
    {
        WiFiClient incoming = _server.available();
        bool placed = false;
        for (int i = 0; i < TCP_MAX_CLIENTS; i++)
        {
            if (!_clients[i] || !_clients[i].connected())
            {
                _clients[i] = incoming;
                _clients[i].setNoDelay(true);
                placed = true;
                if (_logCallback)
                {
                    _logCallback("[TCP] Client connected from %s", _clients[i].remoteIP().toString().c_str());
                }
                break;
            }
        }
        if (!placed)
        {
            incoming.stop();
            if (_logCallback)
            {
                _logCallback("[TCP] Rejected client, all %d slots busy", TCP_MAX_CLIENTS);
            }
        }
    }

    // Read RTCM from clients, drop the dead ones
    uint8_t buffer[256];
    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
    {
        if (!_clients[i])
        {
            continue;
        }
        if (!_clients[i].connected())
        {
            _clients[i].stop();
            if (_logCallback)
            {
                _logCallback("[TCP] Client disconnected");
            }
            continue;
        }

        int available = _clients[i].available();
        while (available > 0)
        {
            int read = _clients[i].read(buffer, min(available, (int)sizeof(buffer)));
            if (read <= 0)
            {
                break;
            }
            if (_dataCallback)
            {
                _dataCallback(buffer, (size_t)read);
            }
            available = _clients[i].available();
        }
    }
}

void TcpTransport::sendData(const uint8_t *data, size_t length)
{
    if (length == 0)
    {
        return;
    }
    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
    {
        if (_clients[i] && _clients[i].connected())
        {
            _clients[i].write(data, length);
        }
    }
}

bool TcpTransport::isConnected()
{
    return getConnectedCount() > 0;
}

uint32_t TcpTransport::getConnectedCount()
{
    uint32_t count = 0;
    for (int i = 0; i < TCP_MAX_CLIENTS; i++)
    {
        if (_clients[i] && _clients[i].connected())
        {
            count++;
        }
    }
    return count;
}
