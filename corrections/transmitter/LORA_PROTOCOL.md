# LoRa RTCM Transmission Protocol

This document describes the packet format used for transmitting RTCM correction data over LoRa.

## Packet Structure

Each LoRa packet follows this format:

```
+------------+---------------+--------------+-------------+------------------+
| Message ID | Total Packets | Packet Number| Data Length |      Data        |
|  (1 byte)  |   (1 byte)    |   (1 byte)   |  (1 byte)   |  (0-246 bytes)   |
+------------+---------------+--------------+-------------+------------------+
```

### Header Fields (4 bytes total)

1. **Message ID** (1 byte): Unique identifier for this message (0-255, wraps around)
2. **Total Packets** (1 byte): Total number of packets for this complete message
3. **Packet Number** (1 byte): Sequential packet number (0 to Total Packets - 1)
4. **Data Length** (1 byte): Number of data bytes in this packet (0-246)

### Data Field (0-246 bytes)

Contains the actual RTCM data payload for this packet.

## Transmission Parameters

- **Frequency**: 433 MHz (configurable via LORA_FREQUENCY)
- **Spreading Factor**: 7
- **Bandwidth**: 125 kHz
- **Coding Rate**: 4/5
- **TX Power**: 20 dBm
- **Sync Word**: 0x12
- **CRC**: Enabled

## Frame Skipping (Radio Link Budget Optimization)

To optimize radio link usage and reduce transmission overhead, the system supports configurable frame skipping:

- **Configuration**: `loraTransmissionRatio` in `config.cpp`
- **Default**: 1 (transmit all frames)
- **Options**:
  - `1` = Transmit all RTCM frames (100% transmission)
  - `2` = Transmit every 2nd frame (50% reduction)
  - `3` = Transmit every 3rd frame (66% reduction)
  - `N` = Transmit every Nth frame

### Benefits

- Reduces LoRa airtime usage
- Extends battery life
- Allows more devices to share the same frequency
- Maintains acceptable correction accuracy for most applications

### Statistics

The system tracks and displays:

- Total RTCM frames received from NTRIP
- Total RTCM frames transmitted via LoRa
- Current transmission ratio
- Total bytes transmitted

## Fragmentation

Large RTCM messages are automatically fragmented into multiple LoRa packets:

- Maximum packet size: 250 bytes
- Header size: 4 bytes
- Maximum data per packet: 246 bytes
- Maximum message size: ~62.7 KB (255 packets × 246 bytes)

## Example

For a 500-byte RTCM message with ID 42:

```
Packet 0: [42][03][00][246][246 bytes of data]
Packet 1: [42][03][01][246][246 bytes of data]
Packet 2: [42][03][02][008][8 bytes of data]
```

## Reception (for future receiver implementation)

Receivers should:

1. Listen for packets with the expected sync word
2. Parse the header to determine message structure
3. Buffer packets until all packets for a message ID are received
4. Reassemble the complete RTCM message
5. Forward to GPS/GNSS receiver

## Error Handling

- Failed packet transmission is logged but not retried
- No acknowledgment protocol (fire-and-forget)
- CRC errors are handled by the LoRa hardware
- Duplicate or out-of-order packets should be handled by receiver
