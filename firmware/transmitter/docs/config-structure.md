# Configuration Structure — Web UI

This document describes the configuration structure of the RTCM LoRa Transmitter device as exposed through the Web UI.
It serves as input for the work on redesigning the interface in Figma.

---

## UI Navigation

The interface is divided into **5 tabs**:

| #   | Tab ID  | Label          | Description                    |
| --- | ------- | -------------- | ------------------------------ |
| 1   | `wifi`  | WiFi           | WiFi connection configuration  |
| 2   | `ntrip` | NTRIP          | NTRIP caster configuration     |
| 3   | `gga`   | GGA Position   | Base station position (GGA)    |
| 4   | `lora`  | LoRa Radio     | LoRa radio parameters (SX1276) |
| 5   | `rtcm`  | RTCM Filtering | RTCM message type filtering    |

### Global actions (outside the tabs)

| Action         | Type               | Behavior                                                   |
| -------------- | ------------------ | ---------------------------------------------------------- |
| Save & Restart | Submit (primary)   | Validation → confirm dialog → POST config → device restart |
| Reload Config  | Button (secondary) | Fetches the current configuration from the device          |
| Restart Device | Button (danger)    | Confirm dialog → device restart                            |

---

## Configuration sections — field details

### 1. WiFi

| Field               | JSON Key        | Type                | Required | Validation                        | Default value | Notes                                                                 |
| ------------------- | --------------- | ------------------- | -------- | --------------------------------- | ------------- | --------------------------------------------------------------------- |
| WiFi Network (SSID) | `wifi.ssid`     | `string`            | ✅       | Non-empty                         | `""`          | Has a "Scan" action — opens a modal with a list of available networks |
| Password            | `wifi.password` | `string` (password) | ❌       | —                                 | `""`          | Empty = open network                                                  |
| Connection Timeout  | `wifi.timeout`  | `integer` (ms)      | ✅       | min: 5000, max: 60000, step: 1000 | `30000`       |                                                                       |

**Additional functionality**: WiFi scan modal

- Displays a list of networks sorted by RSSI (signal strength)
- Each network shows: SSID, BSSID, channel, RSSI (dBm), encryption type (open/secured), signal strength icon
- Clicking a network inserts the SSID into the form field
- The Rescan button allows re-running the scan

### 2. NTRIP

| Field              | JSON Key           | Type                | Required | Validation                        | Default value        | Notes                                                           |
| ------------------ | ------------------ | ------------------- | -------- | --------------------------------- | -------------------- | --------------------------------------------------------------- |
| Host               | `ntrip.host`       | `string`            | ✅       | Non-empty                         | `"91.198.76.2"`      |                                                                 |
| Port               | `ntrip.port`       | `integer`           | ✅       | min: 1, max: 65535                | `2101`               |                                                                 |
| Username           | `ntrip.user`       | `string`            | ❌       | —                                 | `""`                 |                                                                 |
| Password           | `ntrip.password`   | `string` (password) | ❌       | —                                 | `""`                 |                                                                 |
| Mountpoint         | `ntrip.mountpoint` | `string`            | ✅       | Non-empty                         | `"RTN4G_VRS_RTCM32"` | Has a "Fetch" action — opens a modal with a list of mountpoints |
| Connection Timeout | `ntrip.timeout`    | `integer` (ms)      | ✅       | min: 5000, max: 60000, step: 1000 | `10000`              |                                                                 |

**Additional functionality**: NTRIP mountpoint fetch modal

- Requires the Host and Port fields to be filled in first (validation before opening)
- Sends a request to the NTRIP caster with the provided credentials
- Displays a list of mountpoint names
- Clicking a mountpoint inserts its name into the form field
- The Refresh button allows re-running the fetch

### 3. GGA Position

| Field              | JSON Key           | Type           | Required | Validation                        | Default value | Notes                                        |
| ------------------ | ------------------ | -------------- | -------- | --------------------------------- | ------------- | -------------------------------------------- |
| Latitude (°)       | `gga.latitude`     | `float`        | ✅       | min: -90, max: 90, step: 0.0001   | `50.2346`     | Decimal degrees, positive = North            |
| Longitude (°)      | `gga.longitude`    | `float`        | ✅       | min: -180, max: 180, step: 0.0001 | `19.2084`     | Decimal degrees                              |
| Altitude (m)       | `gga.altitude`     | `float`        | ✅       | step: 0.1                         | `266.0`       | Meters above sea level                       |
| Send Interval (ms) | `gga.sendInterval` | `integer` (ms) | ✅       | min: 1000, max: 60000, step: 1000 | `2000`        | Interval for sending GGA to the NTRIP caster |

### 4. LoRa Radio

| Field            | JSON Key               | Type                  | Required | Validation                  | Default value         | Notes                                                |
| ---------------- | ---------------------- | --------------------- | -------- | --------------------------- | --------------------- | ---------------------------------------------------- |
| Frequency (MHz)  | `lora.frequency`       | `integer` (Hz in API) | ✅       | min: 137 MHz, max: 1020 MHz | `433000000` (433 MHz) | UI displays MHz, API stores Hz                       |
| Spreading Factor | `lora.spreadingFactor` | `integer` (enum)      | ✅       | 6–12                        | `7`                   | Select: SF6, SF7, SF8, SF9, SF10, SF11, SF12         |
| Bandwidth (Hz)   | `lora.bandwidth`       | `integer` (enum)      | ✅       | Values from the list        | `125000`              | Select — see table below                             |
| Coding Rate      | `lora.codingRate`      | `integer` (enum)      | ✅       | 5–8                         | `5`                   | Select: 4/5, 4/6, 4/7, 4/8                           |
| TX Power (dBm)   | `lora.txPower`         | `integer`             | ✅       | min: 2, max: 20             | `20`                  |                                                      |
| Sync Word (hex)  | `lora.syncWord`        | `integer` (hex)       | ✅       | Format: `0x[0-9A-Fa-f]{2}`  | `0x12`                | UI displays hex (e.g. `0x12`), API stores an integer |

**Allowed Bandwidth values**:

| Value (Hz) | UI Label  |
| ---------- | --------- |
| 7800       | 7.8 kHz   |
| 10400      | 10.4 kHz  |
| 15600      | 15.6 kHz  |
| 20800      | 20.8 kHz  |
| 31250      | 31.25 kHz |
| 41700      | 41.7 kHz  |
| 62500      | 62.5 kHz  |
| 125000     | 125 kHz   |
| 250000     | 250 kHz   |
| 500000     | 500 kHz   |

### 5. RTCM Filtering

| Field                 | JSON Key                    | Type             | Required | Validation    | Default value                                                  | Notes                                                                 |
| --------------------- | --------------------------- | ---------------- | -------- | ------------- | -------------------------------------------------------------- | --------------------------------------------------------------------- |
| Allowed Message Types | `rtcm.messageTypes`         | `array<integer>` | ✅       | Min 1 element | `[1005, 1007, 1019, 1020, 1033, 1075, 1085, 1095, 1125, 1230]` | UI: textarea, comma-separated                                         |
| High Priority Types   | `rtcm.priorityMessageTypes` | `array<integer>` | ❌       | —             | `[1005, 1075, 1085, 1095, 1125, 1230]`                         | UI: textarea, comma-separated; Types transmitted with higher priority |

---

## API Contract

All endpoints are available over HTTP on port 80 (Access Point).

### `GET /api/config` — Fetch configuration

**Response** `200 OK` — `application/json`

```json
{
  "wifi": {
    "ssid": "string",
    "password": "string",
    "timeout": 30000
  },
  "ntrip": {
    "host": "string",
    "port": 2101,
    "mountpoint": "string",
    "user": "string",
    "password": "string",
    "timeout": 10000
  },
  "gga": {
    "latitude": 50.2346,
    "longitude": 19.2084,
    "altitude": 266.0,
    "sendInterval": 2000
  },
  "lora": {
    "frequency": 433000000,
    "spreadingFactor": 7,
    "bandwidth": 125000,
    "codingRate": 5,
    "txPower": 20,
    "syncWord": 18
  },
  "rtcm": {
    "messageTypes": [
      1005, 1007, 1019, 1020, 1033, 1075, 1085, 1095, 1125, 1230
    ],
    "priorityMessageTypes": [1005, 1075, 1085, 1095, 1125, 1230]
  },
  "display": {
    "updateInterval": 500
  }
}
```

> **Note**: The `display` section is present in the API but is not exposed in the current UI.

### `POST /api/config` — Save configuration

**Request** — `Content-Type: application/json`

Body: the same structure as the response from `GET /api/config`. All sections are optional — the sections sent overwrite the existing configuration, while omitted sections remain unchanged.

**Response** `200 OK`:

```json
{ "success": true, "message": "Configuration saved. Device will restart." }
```

**Response** `400 Bad Request`:

```json
{ "success": false, "message": "Invalid JSON" }
```

```json
{ "success": false, "message": "No data received" }
```

**Response** `500 Internal Server Error`:

```json
{ "success": false, "message": "Failed to save configuration" }
```

> **Behavior**: After a successful save, the device automatically restarts after ~1s.

### `POST /api/restart` — Restart the device

**Request**: Empty body.

**Response** `200 OK`:

```json
{ "success": true, "message": "Device restarting..." }
```

### `POST /api/wifi/scan` — Scan WiFi networks

**Request**: Empty body.

**Response** `200 OK`:

```json
{
  "success": true,
  "networks": [
    {
      "ssid": "MyNetwork",
      "bssid": "AA:BB:CC:DD:EE:FF",
      "rssi": -45,
      "encryption": "secured",
      "channel": 6
    }
  ]
}
```

| Field        | Type      | Description                                            |
| ------------ | --------- | ------------------------------------------------------ |
| `ssid`       | `string`  | Network name                                           |
| `bssid`      | `string`  | Access point MAC address (format: `XX:XX:XX:XX:XX:XX`) |
| `rssi`       | `integer` | Signal strength in dBm (higher = better)               |
| `encryption` | `string`  | `"open"` or `"secured"`                                |
| `channel`    | `integer` | WiFi channel                                           |

**Response** `500`:

```json
{ "success": false, "message": "Scan failed" }
```

### `POST /api/ntrip/mountpoints` — Fetch NTRIP mountpoints

**Request** — `Content-Type: application/x-www-form-urlencoded`

| Parameter  | Required | Description               |
| ---------- | -------- | ------------------------- |
| `host`     | ✅       | NTRIP caster host address |
| `port`     | ✅       | Caster port               |
| `user`     | ❌       | Login (if required)       |
| `password` | ❌       | Password (if required)    |

**Response** `200 OK`:

```json
{
  "mountpoints": ["RTN4G_VRS_RTCM32", "NEAREST_RTN", "VRS_3_2"]
}
```

**Response** `200 OK` (error):

```json
{ "error": "Connection failed" }
```

**Response** `400 Bad Request`:

```json
{ "error": "Missing host or port parameter" }
```

---

## Validation (UI-side)

Validation happens before the form is submitted. List of rules:

| Rule                                     | Error message                                       |
| ---------------------------------------- | --------------------------------------------------- |
| `wifi.ssid` is empty                     | "WiFi SSID is required"                             |
| `ntrip.host` is empty                    | "NTRIP host is required"                            |
| `ntrip.port` < 1 or > 65535              | "NTRIP port must be between 1 and 65535"            |
| `gga.latitude` < -90 or > 90             | "Latitude must be between -90 and 90"               |
| `gga.longitude` < -180 or > 180          | "Longitude must be between -180 and 180"            |
| `lora.frequency` < 137 MHz or > 1020 MHz | "LoRa frequency out of valid range (137–1020 MHz)"  |
| `lora.spreadingFactor` < 6 or > 12       | "Spreading factor must be between 6 and 12"         |
| `lora.txPower` < 2 or > 20               | "TX power must be between 2 and 20 dBm"             |
| `rtcm.messageTypes` is empty             | "At least one RTCM message type must be configured" |

> **Note**: Validation exists only on the UI side. The backend does not validate values — it accepts whatever it receives in valid JSON.

---

## Data conversions (UI ↔ API)

| Field                       | UI format                                    | API format                              | Conversion                                                     |
| --------------------------- | -------------------------------------------- | --------------------------------------- | -------------------------------------------------------------- |
| `lora.frequency`            | MHz (e.g. `433.085`)                         | Hz (e.g. `433085000`)                   | UI→API: `× 1e6`, API→UI: `÷ 1e6`                               |
| `lora.syncWord`             | Hex string (e.g. `0x12`)                     | Integer (e.g. `18`)                     | UI→API: `parseInt(val, 16)`, API→UI: `"0x" + val.toString(16)` |
| `rtcm.messageTypes`         | Comma-separated string (e.g. `"1005, 1075"`) | Array of integers (e.g. `[1005, 1075]`) | UI→API: split + parseInt, API→UI: join                         |
| `rtcm.priorityMessageTypes` | Comma-separated string                       | Array of integers                       | same as above                                                  |

---

## Network and Access Point

- The device starts an Access Point named `OSS-LoRa-TX-XXXX` (where XXXX = the last 4 characters of the MAC)
- The AP has no password (open network)
- AP timeout: 3 minutes without a client → the AP shuts down
- After a client connects and then disconnects: 3 minutes → the AP shuts down
- The Web UI is available at `http://192.168.4.1/`
