#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

// System states (dual-core architecture)
enum SystemState
{
    STATE_INIT,
    STATE_RUNNING,
    STATE_MAINTENANCE_TX, // Transmitting maintenance frame (Core 0)
    STATE_MAINTENANCE_RX, // Listening for incoming command in RX mode (Core 0)
    STATE_STANDBY,        // RTCM TX halted, LoRa in continuous RX — awaiting CMD_START
    STATE_ERROR
};

#endif // SYSTEM_STATE_H
