# LoRa RTCM Transmission Protocol

## Overview

RTCM3 correction data is transmitted over LoRa using a simple fragmentation protocol. Large RTCM messages are split into multiple LoRa packets and reassembled at the receiver.

## Protocol Version

This document describes **Protocol v2**. The header was extended by one byte (`frameType`) compared to the original 4-byte header.

| `frameType` | Symbol | Description |
|-------------|--------|-------------|
| `0x01` | `FRAME_RTCM` | RTCM correction data |
| `0x02` | `FRAME_MAINTENANCE` | Telemetry broadcast (TX → air) |
| `0x03` | `FRAME_COMMAND` | Control command (RX → transmitter) |
| `0x04` | `FRAME_ACK` | Optional acknowledgement |

## Packet Structure

```
+────────────+──────────────+─────────────────+────────────────+─────────────+──────────────────+
│ Frame Type │  Message ID  │  Total Packets  │  Packet Number │ Data Length │       Data       │
│  (1 byte)  │   (1 byte)   │    (1 byte)     │    (1 byte)    │  (1 byte)   │  (0-245 bytes)   │
+────────────+──────────────+─────────────────+────────────────+─────────────+──────────────────+
```

### Header Fields (5 bytes)

| Field         | Size   | Range        | Description                            |
| ------------- | ------ | ------------ | -------------------------------------- |
| Frame Type    | 1 byte | 0x01–0x04    | Type of frame (see table above)        |
| Message ID    | 1 byte | 0–255        | Unique ID per message, wraps around    |
| Total Packets | 1 byte | 1–255        | How many packets comprise this message |
| Packet Number | 1 byte | 0 to Total-1 | Sequential index of this packet        |
| Data Length   | 1 byte | 0–245        | Payload bytes in this packet           |

### Payload

Up to 245 bytes of raw RTCM data per packet (250 byte max LoRa frame − 5 byte header).

## Radio Parameters

| Parameter        | Value              |
| ---------------- | ------------------ |
| Frequency        | 444.2 MHz          |
| Spreading Factor | SF7                |
| Bandwidth        | 500 kHz            |
| Coding Rate      | 4/5                |
| TX Power         | 20 dBm             |
| Sync Word        | 0x12               |
| Preamble         | 12 symbols         |
| CRC              | Enabled (hardware) |

## Fragmentation Example

A 500-byte RTCM message with Message ID 42:

```
Packet 0: [0x01] [42] [03] [00] [245] [245 bytes of RTCM data]
Packet 1: [0x01] [42] [03] [01] [245] [245 bytes of RTCM data]
Packet 2: [0x01] [42] [03] [02] [010] [ 10 bytes of RTCM data]
```

Maximum theoretical message size: 255 × 245 = 62,475 bytes.

## Reassembly at Receiver

```
Incoming LoRa Packet
        │
        ▼
  Parse header (frameType, msgId, total, pktNum, len)
        │
        ▼
  frameType == FRAME_RTCM? If not, drain and discard
        │
        ▼
  Validate: pktNum < total, total > 0, len ≤ 245
        │
        ▼
  Lookup/create MessageReassembly[msgId]
        │
        ▼
  Store packet data at pktNum slot
        │
        ▼
  receivedPackets == totalPackets?
       ╱              ╲
    Yes                No
     │                  │
     ▼                  ▼
  Reassemble in       Wait for more
  packet order        (timeout: 5s)
     │
     ▼
  Feed to RTCM parser
```

### Timeout Handling

- Incomplete messages are cleaned up after `PACKET_TIMEOUT` (5 seconds)
- Timeout counter is incremented for statistics
- Duplicate packets are silently discarded

### Validation

The receiver checks:

1. Packet size ≥ 5 bytes (header) and ≤ 250 bytes
2. `frameType == FRAME_RTCM` (`0x01`) — other types are drained and discarded
3. `packetNumber < totalPackets`
4. `totalPackets > 0`
5. `dataLength ≤ 245`
6. Consistent `totalPackets` across all packets of the same message

## RTCM3 Frame Parsing

After reassembly, the raw byte stream is fed into a circular buffer and parsed for RTCM3 frames:

```
┌──────────┬───────────────┬─────────────────┬───────────┐
│ Preamble │ Reserved + Len│    Payload      │  CRC-24Q  │
│  0xD3    │   2 bytes     │  0-1023 bytes   │  3 bytes  │
│ (1 byte) │ (10+10 bits)  │                 │           │
└──────────┴───────────────┴─────────────────┴───────────┘
```

- **Preamble**: Always `0xD3`
- **Length**: 10-bit value in bits 14–23 of the header (max 1023)
- **Message Type**: First 12 bits of payload
- **Station ID**: Next 12 bits of payload
- **CRC-24Q**: Qualcomm CRC over header + payload

### Supported RTCM Message Types

| Type      | Description                |
| --------- | -------------------------- |
| 1001–1004 | GPS L1/L2 observations     |
| 1005–1006 | Station coordinates        |
| 1007–1008 | Antenna description        |
| 1009–1012 | GLONASS L1/L2 observations |
| 1019      | GPS ephemeris              |
| 1020      | GLONASS ephemeris          |
| 1033      | Receiver information       |
| 1074–1077 | GPS MSM4/5/7               |
| 1084–1087 | GLONASS MSM4/5/7           |
| 1094–1097 | Galileo MSM4/5/7           |
| 1124–1127 | BeiDou MSM4/5/7            |
| 1230      | GLONASS code-phase bias    |

## Error Statistics

The receiver tracks:

| Counter                    | Description                               |
| -------------------------- | ----------------------------------------- |
| `completeMessagesReceived` | Successfully reassembled messages         |
| `timeoutMessages`          | Messages that timed out (missing packets) |
| `crcErrorMessages`         | RTCM frames with invalid CRC-24Q          |
| `lastRssi`                 | RSSI of last received LoRa packet (dBm)   |
| Message rate               | Complete messages per second (10s window) |

## Design Decisions

- **Fire-and-forget**: No ACK/retransmission — LoRa link budget and hardware CRC provide sufficient reliability
- **Simple header**: 4-byte overhead enables maximum payload per packet
- **Wrapping Message ID**: 8-bit ID limits to 256 concurrent in-flight messages (sufficient for RTCM rates)
