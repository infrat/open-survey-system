# Rover — Bill of Materials

Portable dual-frequency GNSS RTK receiver with a Bluetooth link to a mobile
app. This BOM is derived from the KiCad schematic
([`kicad/rover.kicad_sch`](kicad/rover.kicad_sch)) and is the authoritative
parts list for the custom rover PCB. A machine-readable copy is in
[`bom.csv`](bom.csv).

> The schematic uses an **ESP32-C3 (Wemos C3 mini)** MCU plus an **HC-05**
> classic Bluetooth SPP module over UART. (The rover firmware README also
> documents a BLE-only variant on a Wemos D1 Mini32; the schematic below is the
> built reference design.)

## Modules / sockets

| Ref | Qty | Part                              | Description                              | Notes                               | Est. USD |
| --- | --- | --------------------------------- | ---------------------------------------- | ----------------------------------- | -------- |
| U2  | 1   | Wemos C3 mini (ESP32-C3)          | Main MCU — UART↔BLE/SPP bridge, OTA      | `RF_Module:WEMOS_C3_mini` footprint | 4–6      |
| J1  | 1   | Quectel LC29H (DA) socket         | Dual-band, dual-antenna RTK GNSS module  | Mounted on `LC29H` socket           | 35–55    |
| J2  | 1   | HC-05 socket                      | Classic Bluetooth SPP module (UART)      | `HC05` socket                       | 4–7      |
| J3  | 1   | BMS FM5324 socket                 | Li-ion battery management / power module | `BMS_FM5324` socket                 | 3–6      |
| J4  | 1   | Battery connector (1x02, 2.54 mm) | Li-ion cell input                        | PinHeader 1x02 horizontal           | <1       |
| J5  | 1   | Control-panel socket              | Buttons / status LED breakout            | `RTK_ControlPanel` socket           | 1–2      |

## Passives

| Ref        | Qty | Value  | Package                                | Notes                 |
| ---------- | --- | ------ | -------------------------------------- | --------------------- |
| C1, C2, C5 | 3   | 100 nF | Disc THT (D3.0 mm, P2.5 mm)            | MCU/module decoupling |
| C7         | 1   | 10 µF  | Radial electrolytic (D4.0 mm, P2.0 mm) | Bulk decoupling       |
| C6         | 1   | 100 µF | Radial electrolytic (D5.0 mm, P2.5 mm) | Bulk / power rail     |
| R1, R2     | 2   | 300 Ω  | Axial THT (DIN0204)                    | LED / current-limit   |

## Off-board / mechanical (not on schematic)

| Qty | Part                                                   | Description                            | Est. USD |
| --- | ------------------------------------------------------ | -------------------------------------- | -------- |
| 1   | Li-ion battery (e.g. 18650, ~3000 mAh, 3.7 V)          | ~20–30 h runtime per firmware estimate | 5–9      |
| 1   | GNSS antenna (L1/L2/L5 multi-band, e.g. helical/patch) | Required for LC29H DA RTK              | 20–40    |
| 1   | Antenna cable / SMA pigtail                            | Match module connector (U.FL→SMA)      | 2–4      |
| 1   | Enclosure                                              | See `imgs/enclosure*.png` in repo root | 5–15     |
| 1   | USB-C cable                                            | Programming / charging                 | 2–4      |
| —   | PCB fabrication                                        | See [`gerber/`](gerber/)               | 2–10     |

**Indicative total (excl. enclosure/antenna):** ~$60–100 depending on the GNSS
module and supplier.

## Pinout reference (ESP32 ↔ LC29H DA)

```
LC29H DA UART1_TX → ESP32 GPIO25 (RX)
LC29H DA UART1_RX ← ESP32 GPIO27 (TX)
VCC 3.3–5V (via power module), common GND
Baud 115200, 8N1, no flow control
```
