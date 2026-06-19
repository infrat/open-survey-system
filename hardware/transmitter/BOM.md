# Transmitter (LTE/LoRa Gateway) — Bill of Materials

ESP32 + SX1276 gateway that fetches RTCM3 corrections from an NTRIP caster over
WiFi and retransmits them via **LoRa 433 MHz** to the receiver. Built around a
commodity LoRa development board (no custom PCB), so this BOM lists ready-made
modules and the few external parts that complete the build.

## Core

| Qty | Part                                                         | Description                             | Notes                                                            | Est. USD |
| --- | ------------------------------------------------------------ | --------------------------------------- | ---------------------------------------------------------------- | -------- |
| 1   | LoRa dev board: **TTGO T-Beam V1.1** (recommended) or LoRa32 | ESP32 + SX1276 (433 MHz) + SSD1306 OLED | T-Beam adds GPS + 18650 holder + PMU; firmware env `ttgo-t-beam` | 22–35    |
| 1   | 433 MHz LoRa antenna                                         | SMA dipole; yagi/directional for range  | Must be 433 MHz, not 868/915                                     | 3–10     |
| 1   | 18650 Li-ion cell (3.7 V)                                    | Field power; holder is on the T-Beam    | Or power via USB-C                                               | 5–9      |
| 1   | USB-C cable                                                  | Programming / power                     | —                                                                | 2–4      |

## Optional / mechanical

| Qty | Part                                | Description                                                          | Est. USD |
| --- | ----------------------------------- | -------------------------------------------------------------------- | -------- |
| 1   | SSD1306 128x64 OLED                 | Only if the board lacks one (LoRa32 variants)                        | 3–5      |
| 1   | WiFi/LTE uplink                     | Onboard ESP32 WiFi, or external LTE router/hotspot for internet path | varies   |
| 1   | Enclosure (weatherproof if outdoor) | Field deployment                                                     | 5–20     |
| 1   | SMA pigtail / extension             | Mount antenna externally                                             | 2–5      |

**Indicative total:** ~$35–60 (T-Beam + 433 MHz antenna + battery).

## SX1276 pinout (T-Beam predefined)

```
NSS  → GPIO18    MOSI → GPIO27    MISO → GPIO19
SCK  → GPIO5     RST  → GPIO23    DIO0 → GPIO26
```

## OLED (SSD1306 128x64)

```
I2C: SDA → GPIO21, SCL → GPIO22
```

## LoRa radio parameters (must match receiver)

| Parameter        | Value                                                       |
| ---------------- | ----------------------------------------------------------- |
| Frequency        | 433 MHz (433.7 MHz to match receiver default)               |
| Spreading factor | 7                                                           |
| Bandwidth        | 125 kHz (receiver default is 500 kHz — keep both identical) |
| Coding rate      | 4/5                                                         |
| TX power         | 20 dBm (SX1276 max)                                         |
| Sync word        | 0x12                                                        |

> ⚠️ The LoRa radio parameters (frequency, SF, bandwidth, sync word) must be
> identical on transmitter and receiver or the link fails silently.
