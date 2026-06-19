# Receiver (LoRa/Wi‑Fi Gateway) — Bill of Materials

ESP32 + SX1276 gateway that receives RTCM3 corrections over a **433 MHz LoRa**
link, reassembles them, and re-serves them to nearby GNSS rovers through a local
NTRIP caster on its own WiFi access point. Built around a commodity LoRa
development board (no custom PCB), so this BOM lists ready-made modules and the
few external parts that complete the build.

## Core

| Qty | Part                                      | Description                             | Notes                           | Est. USD |
| --- | ----------------------------------------- | --------------------------------------- | ------------------------------- | -------- |
| 1   | LoRa dev board: **TTGO LoRa32 V2.1**      | ESP32 + SX1276 (433 MHz) + SSD1306 OLED | Firmware env `ttgo-lora32-v21`  | 18–28    |
| 1   | 433 MHz LoRa antenna                      | SMA dipole; yagi/directional for range  | Must be 433 MHz, not 868/915    | 3–10     |
| 1   | Passive buzzer                            | Audible pulse on every valid RTCM frame | Driven from **GPIO4**           | <1       |
| 1   | 18650 Li-ion cell (3.7 V) or USB-C supply | Field power                             | LoRa32 V2.1 has an 18650 holder | 5–9      |
| 1   | USB-C cable                               | Programming / power                     | —                               | 2–4      |

## Optional / mechanical

| Qty | Part                                | Description                                     | Est. USD |
| --- | ----------------------------------- | ----------------------------------------------- | -------- |
| 1   | SSD1306 128x64 OLED                 | Only if board variant lacks the onboard display | 3–5      |
| 1   | Enclosure (weatherproof if outdoor) | Field deployment                                | 5–20     |
| 1   | SMA pigtail / extension             | Mount antenna externally                        | 2–5      |

**Indicative total:** ~$30–50 (LoRa32 V2.1 + 433 MHz antenna + buzzer + battery).

## SX1276 pinout (TTGO LoRa32 V2.1)

```
NSS  → GPIO18    MOSI → GPIO27    MISO → GPIO19
SCK  → GPIO5     RST  → GPIO14    DIO0 → GPIO26
```

## OLED (SSD1306 128x64)

```
I2C: SDA → GPIO21, SCL → GPIO22, RST → GPIO16
```

## Buzzer

```
Signal → GPIO4 (non-blocking pulse on valid RTCM frame)
```

## LoRa radio parameters (must match transmitter)

| Parameter        | Value                   |
| ---------------- | ----------------------- |
| Frequency        | 433.7 MHz (`433700000`) |
| Spreading factor | 7                       |
| Bandwidth        | 500 kHz                 |
| Coding rate      | 4/5                     |
| TX power         | 20 dBm                  |
| Sync word        | 0x12                    |

> ⚠️ These radio parameters must be identical to the transmitter or the link
> fails silently.
