#ifndef TRANSPORT_FACTORY_H
#define TRANSPORT_FACTORY_H

#include "settings.h"
#include "transport.h"

// Creates the transport selected in the settings. Called once per boot: the
// BLE and SPP hosts share one Bluetooth controller, so switching between them
// (or to TCP) always goes through a restart.
ITransport *createTransport(const RoverSettings &settings);

#endif // TRANSPORT_FACTORY_H
