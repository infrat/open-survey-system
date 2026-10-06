#ifndef ROVER_TRANSPORT_H
#define ROVER_TRANSPORT_H

#include "config.h"
#include "transport.h"

// ---------------------------------------------------------------------------
// Compile-time transport selection. Only the selected header is pulled in, so
// the unused Bluetooth stack is never referenced by the application code.
//
//   ROVER_TRANSPORT == TRANSPORT_BLE  → BLE Nordic UART Service (NimBLE)
//   ROVER_TRANSPORT == TRANSPORT_SPP  → Bluetooth Classic SPP (Bluedroid)
//
// Include this header (not the concrete ones) from application code.
// ---------------------------------------------------------------------------
#if ROVER_TRANSPORT == TRANSPORT_SPP
#include "spp_serial_service.h"
typedef SPPSerialService RoverTransport;
#elif ROVER_TRANSPORT == TRANSPORT_BLE
#include "ble_uart_service.h"
typedef BLEUARTService RoverTransport;
#else
#error "Unsupported ROVER_TRANSPORT value - use TRANSPORT_BLE or TRANSPORT_SPP"
#endif

#endif // ROVER_TRANSPORT_H
