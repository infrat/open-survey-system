# Struktura konfiguracji — Web UI

Dokument opisuje strukturę konfiguracji urządzenia RTCM LoRa Transmitter eksponowaną przez Web UI.
Służy jako input do pracy nad redesignem interfejsu w Figmie.

---

## Nawigacja UI

Interfejs podzielony jest na **5 zakładek (tabów)**:

| #   | Tab ID  | Etykieta       | Opis                              |
| --- | ------- | -------------- | --------------------------------- |
| 1   | `wifi`  | WiFi           | Konfiguracja połączenia WiFi      |
| 2   | `ntrip` | NTRIP          | Konfiguracja castera NTRIP        |
| 3   | `gga`   | GGA Position   | Pozycja stacji bazowej (GGA)      |
| 4   | `lora`  | LoRa Radio     | Parametry radia LoRa (SX1276)     |
| 5   | `rtcm`  | RTCM Filtering | Filtrowanie typów wiadomości RTCM |

### Globalne akcje (poza tabami)

| Akcja          | Typ                | Zachowanie                                                    |
| -------------- | ------------------ | ------------------------------------------------------------- |
| Save & Restart | Submit (primary)   | Walidacja → confirm dialog → POST config → restart urządzenia |
| Reload Config  | Button (secondary) | Pobiera aktualną konfigurację z urządzenia                    |
| Restart Device | Button (danger)    | Confirm dialog → restart urządzenia                           |

---

## Sekcje konfiguracji — szczegóły pól

### 1. WiFi

| Pole                | Klucz JSON      | Typ                 | Wymagane | Walidacja                         | Wartość domyślna | Uwagi                                                         |
| ------------------- | --------------- | ------------------- | -------- | --------------------------------- | ---------------- | ------------------------------------------------------------- |
| WiFi Network (SSID) | `wifi.ssid`     | `string`            | ✅       | Niepuste                          | `""`             | Posiada akcję "Scan" — otwiera modal z listą dostępnych sieci |
| Password            | `wifi.password` | `string` (password) | ❌       | —                                 | `""`             | Puste = sieć otwarta                                          |
| Connection Timeout  | `wifi.timeout`  | `integer` (ms)      | ✅       | min: 5000, max: 60000, step: 1000 | `30000`          |                                                               |

**Dodatkowa funkcjonalność**: Modal skanowania WiFi

- Wyświetla listę sieci posortowaną po RSSI (siła sygnału)
- Każda sieć pokazuje: SSID, BSSID, kanał, RSSI (dBm), typ szyfrowania (open/secured), ikonę siły sygnału
- Kliknięcie na sieć wstawia SSID do pola formularza
- Przycisk Rescan pozwala ponowić skanowanie

### 2. NTRIP

| Pole               | Klucz JSON         | Typ                 | Wymagane | Walidacja                         | Wartość domyślna     | Uwagi                                                      |
| ------------------ | ------------------ | ------------------- | -------- | --------------------------------- | -------------------- | ---------------------------------------------------------- |
| Host               | `ntrip.host`       | `string`            | ✅       | Niepuste                          | `"91.198.76.2"`      |                                                            |
| Port               | `ntrip.port`       | `integer`           | ✅       | min: 1, max: 65535                | `2101`               |                                                            |
| Username           | `ntrip.user`       | `string`            | ❌       | —                                 | `""`                 |                                                            |
| Password           | `ntrip.password`   | `string` (password) | ❌       | —                                 | `""`                 |                                                            |
| Mountpoint         | `ntrip.mountpoint` | `string`            | ✅       | Niepuste                          | `"RTN4G_VRS_RTCM32"` | Posiada akcję "Fetch" — otwiera modal z listą mountpointów |
| Connection Timeout | `ntrip.timeout`    | `integer` (ms)      | ✅       | min: 5000, max: 60000, step: 1000 | `10000`              |                                                            |

**Dodatkowa funkcjonalność**: Modal pobierania mountpointów

- Wymaga wcześniejszego wypełnienia pól Host i Port (walidacja przed otwarciem)
- Wysyła zapytanie do castera NTRIP z podanymi danymi uwierzytelniającymi
- Wyświetla listę nazw mountpointów
- Kliknięcie na mountpoint wstawia jego nazwę do pola formularza
- Przycisk Refresh pozwala ponowić pobieranie

### 3. GGA Position

| Pole               | Klucz JSON         | Typ            | Wymagane | Walidacja                         | Wartość domyślna | Uwagi                                   |
| ------------------ | ------------------ | -------------- | -------- | --------------------------------- | ---------------- | --------------------------------------- |
| Latitude (°)       | `gga.latitude`     | `float`        | ✅       | min: -90, max: 90, step: 0.0001   | `50.2346`        | Stopnie dziesiętne, dodatnie = North    |
| Longitude (°)      | `gga.longitude`    | `float`        | ✅       | min: -180, max: 180, step: 0.0001 | `19.2084`        | Stopnie dziesiętne                      |
| Altitude (m)       | `gga.altitude`     | `float`        | ✅       | step: 0.1                         | `266.0`          | Metry n.p.m.                            |
| Send Interval (ms) | `gga.sendInterval` | `integer` (ms) | ✅       | min: 1000, max: 60000, step: 1000 | `2000`           | Interwał wysyłania GGA do castera NTRIP |

### 4. LoRa Radio

| Pole             | Klucz JSON             | Typ                  | Wymagane | Walidacja                   | Wartość domyślna      | Uwagi                                                  |
| ---------------- | ---------------------- | -------------------- | -------- | --------------------------- | --------------------- | ------------------------------------------------------ |
| Frequency (MHz)  | `lora.frequency`       | `integer` (Hz w API) | ✅       | min: 137 MHz, max: 1020 MHz | `433000000` (433 MHz) | UI wyświetla MHz, API przechowuje Hz                   |
| Spreading Factor | `lora.spreadingFactor` | `integer` (enum)     | ✅       | 6–12                        | `7`                   | Select: SF6, SF7, SF8, SF9, SF10, SF11, SF12           |
| Bandwidth (Hz)   | `lora.bandwidth`       | `integer` (enum)     | ✅       | Wartości z listy            | `125000`              | Select — patrz tabela niżej                            |
| Coding Rate      | `lora.codingRate`      | `integer` (enum)     | ✅       | 5–8                         | `5`                   | Select: 4/5, 4/6, 4/7, 4/8                             |
| TX Power (dBm)   | `lora.txPower`         | `integer`            | ✅       | min: 2, max: 20             | `20`                  |                                                        |
| Sync Word (hex)  | `lora.syncWord`        | `integer` (hex)      | ✅       | Format: `0x[0-9A-Fa-f]{2}`  | `0x12`                | UI wyświetla hex (np. `0x12`), API przechowuje integer |

**Dozwolone wartości Bandwidth**:

| Wartość (Hz) | Etykieta UI |
| ------------ | ----------- |
| 7800         | 7.8 kHz     |
| 10400        | 10.4 kHz    |
| 15600        | 15.6 kHz    |
| 20800        | 20.8 kHz    |
| 31250        | 31.25 kHz   |
| 41700        | 41.7 kHz    |
| 62500        | 62.5 kHz    |
| 125000       | 125 kHz     |
| 250000       | 250 kHz     |
| 500000       | 500 kHz     |

### 5. RTCM Filtering

| Pole                  | Klucz JSON                  | Typ              | Wymagane | Walidacja     | Wartość domyślna                                               | Uwagi                                                                   |
| --------------------- | --------------------------- | ---------------- | -------- | ------------- | -------------------------------------------------------------- | ----------------------------------------------------------------------- |
| Allowed Message Types | `rtcm.messageTypes`         | `array<integer>` | ✅       | Min 1 element | `[1005, 1007, 1019, 1020, 1033, 1075, 1085, 1095, 1125, 1230]` | UI: textarea, comma-separated                                           |
| High Priority Types   | `rtcm.priorityMessageTypes` | `array<integer>` | ❌       | —             | `[1005, 1075, 1085, 1095, 1125, 1230]`                         | UI: textarea, comma-separated; Typy transmitowane z wyższym priorytetem |

---

## Kontrakt API

Wszystkie endpointy dostępne po HTTP na porcie 80 (Access Point).

### `GET /api/config` — Pobranie konfiguracji

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

> **Uwaga**: Sekcja `display` jest obecna w API, ale nie jest eksponowana w obecnym UI.

### `POST /api/config` — Zapis konfiguracji

**Request** — `Content-Type: application/json`

Body: taka sama struktura jak response z `GET /api/config`. Wszystkie sekcje są opcjonalne — wysłane sekcje nadpisują istniejącą konfigurację, pominięte pozostają bez zmian.

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

> **Zachowanie**: Po udanym zapisie urządzenie automatycznie restartuje się po ~1s.

### `POST /api/restart` — Restart urządzenia

**Request**: Pusty body.

**Response** `200 OK`:

```json
{ "success": true, "message": "Device restarting..." }
```

### `POST /api/wifi/scan` — Skanowanie sieci WiFi

**Request**: Pusty body.

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

| Pole         | Typ       | Opis                                                  |
| ------------ | --------- | ----------------------------------------------------- |
| `ssid`       | `string`  | Nazwa sieci                                           |
| `bssid`      | `string`  | Adres MAC access pointa (format: `XX:XX:XX:XX:XX:XX`) |
| `rssi`       | `integer` | Siła sygnału w dBm (wyższa = lepsza)                  |
| `encryption` | `string`  | `"open"` lub `"secured"`                              |
| `channel`    | `integer` | Kanał WiFi                                            |

**Response** `500`:

```json
{ "success": false, "message": "Scan failed" }
```

### `POST /api/ntrip/mountpoints` — Pobranie mountpointów NTRIP

**Request** — `Content-Type: application/x-www-form-urlencoded`

| Parametr   | Wymagany | Opis                      |
| ---------- | -------- | ------------------------- |
| `host`     | ✅       | Adres hosta castera NTRIP |
| `port`     | ✅       | Port castera              |
| `user`     | ❌       | Login (jeśli wymagany)    |
| `password` | ❌       | Hasło (jeśli wymagane)    |

**Response** `200 OK`:

```json
{
  "mountpoints": ["RTN4G_VRS_RTCM32", "NEAREST_RTN", "VRS_3_2"]
}
```

**Response** `200 OK` (błąd):

```json
{ "error": "Connection failed" }
```

**Response** `400 Bad Request`:

```json
{ "error": "Missing host or port parameter" }
```

---

## Walidacja (po stronie UI)

Walidacja odbywa się przed wysłaniem formularza. Lista reguł:

| Reguła                                    | Komunikat błędu                                     |
| ----------------------------------------- | --------------------------------------------------- |
| `wifi.ssid` jest puste                    | "WiFi SSID is required"                             |
| `ntrip.host` jest puste                   | "NTRIP host is required"                            |
| `ntrip.port` < 1 lub > 65535              | "NTRIP port must be between 1 and 65535"            |
| `gga.latitude` < -90 lub > 90             | "Latitude must be between -90 and 90"               |
| `gga.longitude` < -180 lub > 180          | "Longitude must be between -180 and 180"            |
| `lora.frequency` < 137 MHz lub > 1020 MHz | "LoRa frequency out of valid range (137–1020 MHz)"  |
| `lora.spreadingFactor` < 6 lub > 12       | "Spreading factor must be between 6 and 12"         |
| `lora.txPower` < 2 lub > 20               | "TX power must be between 2 and 20 dBm"             |
| `rtcm.messageTypes` jest puste            | "At least one RTCM message type must be configured" |

> **Uwaga**: Walidacja istnieje tylko po stronie UI. Backend nie wykonuje walidacji wartości — przyjmuje to, co dostanie w poprawnym JSON-ie.

---

## Konwersje danych (UI ↔ API)

| Pole                        | Format UI                                   | Format API                             | Konwersja                                                      |
| --------------------------- | ------------------------------------------- | -------------------------------------- | -------------------------------------------------------------- |
| `lora.frequency`            | MHz (np. `433.085`)                         | Hz (np. `433085000`)                   | UI→API: `× 1e6`, API→UI: `÷ 1e6`                               |
| `lora.syncWord`             | Hex string (np. `0x12`)                     | Integer (np. `18`)                     | UI→API: `parseInt(val, 16)`, API→UI: `"0x" + val.toString(16)` |
| `rtcm.messageTypes`         | Comma-separated string (np. `"1005, 1075"`) | Array of integers (np. `[1005, 1075]`) | UI→API: split + parseInt, API→UI: join                         |
| `rtcm.priorityMessageTypes` | Comma-separated string                      | Array of integers                      | j.w.                                                           |

---

## Sieć i Access Point

- Urządzenie startuje Access Point o nazwie `OSS-LoRa-TX-XXXX` (gdzie XXXX = ostatnie 4 znaki MAC)
- AP nie ma hasła (sieć otwarta)
- AP timeout: 3 minuty bez klienta → AP się wyłącza
- Po podłączeniu klienta i jego rozłączeniu: 3 minuty → AP się wyłącza
- Web UI dostępne pod `http://192.168.4.1/`
