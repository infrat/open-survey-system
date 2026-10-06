#include "transport_factory.h"
#include "ble_uart_service.h"
#include "spp_serial_service.h"
#include "tcp_transport.h"

ITransport *createTransport(const RoverSettings &settings)
{
    switch (settings.transport)
    {
    case TransportMode::SPP:
        return new SPPSerialService();
    case TransportMode::TCP:
        return new TcpTransport(settings.tcpPort);
    case TransportMode::BLE:
    default:
        return new BLEUARTService();
    }
}
