# NTRIP Server

## Overview

The device runs a lightweight NTRIP caster accessible over the WiFi Access Point. GNSS rover receivers connect to it to receive real-time RTCM corrections relayed from the LoRa link.

## Network Setup

```
LoRa Transmitter                  OSS Receiver (this device)              GNSS Rover
    ╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶╶
                                  ┌──────────────────┐
    RTCM via LoRa ──────────────► │  WiFi AP         │ ◄─── Rover connects
                                  │  192.168.4.1     │      to WiFi
                                  │                  │
                                  │  NTRIP Caster    │
                                  │  Port 2101       │ ◄─── GET /LORA HTTP/1.1
                                  │  Mount: /LORA    │ ───► ICY 200 OK
                                  │                  │ ───► RTCM binary stream
                                  └──────────────────┘
```

## Connection Details

| Parameter     | Value          |
| ------------- | -------------- |
| WiFi SSID     | `OSS-LoRa-RX`  |
| WiFi Password | `lora123456`   |
| Server IP     | `192.168.4.1`  |
| NTRIP Port    | `2101`         |
| Mount Point   | `LORA`         |
| Max Clients   | 4 simultaneous |

## NTRIP Protocol Flow

### 1. Source Table Request

Client sends:

```
GET / HTTP/1.1
```

Server responds:

```
SOURCETABLE 200 OK
Content-Type: gnss/sourcetable

STR;LORA;LORA;RTCM 3.x;1005,1077,1087,1097,1127,1230;2;GPS+GLO+GAL+BDS;SNIP;ESP32;N;N;560;none
ENDSOURCETABLE
```

Connection is closed after sending the source table.

### 2. Mount Point Stream Request

Client sends:

```
GET /LORA HTTP/1.1
```

Server responds:

```
ICY 200 OK
Content-Type: gnss/data

<binary RTCM data stream>
```

The connection stays open. Complete RTCM frames are pushed to the client as they arrive from the LoRa link.

## Client Lifecycle

```
New TCP connection on port 2101
        │
        ▼
  Assign to free client slot (max 4)
        │
  No free slot? → 503 Service Unavailable → close
        │
        ▼
  Wait for GET request
        │
        ├── GET /LORA → ICY 200 OK → authenticated = true → stream RTCM
        │
        └── GET / HTTP → send source table → close
        │
        ▼
  Stream active (broadcastRTCMData pushes frames)
        │
        ▼
  Client disconnects → slot freed
```

## Rover Configuration

Configure your GNSS rover's NTRIP client with:

| Setting         | Value            |
| --------------- | ---------------- |
| Host / IP       | `192.168.4.1`    |
| Port            | `2101`           |
| Mount Point     | `LORA`           |
| User / Password | _(not required)_ |

### Example: u-blox u-center

1. Connect rover to `OSS-LoRa-RX` WiFi
2. Receiver → NTRIP Client
3. Address: `192.168.4.1:2101`
4. Mount point: `LORA`

### Example: SW Maps (Android)

1. Connect phone to `OSS-LoRa-RX` WiFi
2. NTRIP Connection → Add
3. Host: `192.168.4.1`, Port: `2101`, Mount: `LORA`

## OLED Display

The display shows live statistics updated every 500ms:

```
┌────────────────────────────┐
│ OSS RTCM Receiver          │
│────────────────────────────│
│ RX:          2.3/s (1s)    │
│ Err:              0.5%     │
│ WiFi:             1/4      │
│ NTRIP:            1/4      │
└────────────────────────────┘
```

| Line  | Meaning                                                  |
| ----- | -------------------------------------------------------- |
| RX    | RTCM message rate (msg/s) and seconds since last message |
| Err   | Error rate (timeouts + CRC failures as % of total)       |
| WiFi  | Connected WiFi stations / max                            |
| NTRIP | Authenticated NTRIP clients / max                        |
