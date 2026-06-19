# OSS Hardware

Bill of Materials (BOM) and fabrication files for each building block of the
Open Survey System (OSS).

| Component                   | Role                                                   | BOM                                      |
| --------------------------- | ------------------------------------------------------ | ---------------------------------------- |
| [Rover](rover/)             | Portable GNSS RTK receiver (GNSS + BLE/SPP bridge)     | [rover/BOM.md](rover/BOM.md)             |
| [Transmitter](transmitter/) | LTE/LoRa Gateway — NTRIP client → LoRa 433 MHz uplink  | [transmitter/BOM.md](transmitter/BOM.md) |
| [Receiver](receiver/)       | LoRa/Wi‑Fi Gateway — LoRa 433 MHz → local NTRIP caster | [receiver/BOM.md](receiver/BOM.md)       |

## Notes on the BOMs

- **Rover** is a custom PCB. Its BOM is derived directly from the KiCad
  schematic in [`rover/kicad/rover.kicad_sch`](rover/kicad/rover.kicad_sch) and
  is the authoritative parts list. A machine-readable copy lives in
  [`rover/bom.csv`](rover/bom.csv).
- **Transmitter** and **Receiver** are built around commodity ESP32 + SX1276
  LoRa development boards (no custom PCB yet), so their BOMs list ready-made
  modules and the few external parts that complete the build.
- Prices are rough indications (USD) for low-volume hobby quantities and will
  vary by region and supplier. Treat them as planning estimates, not quotes.
- Region-aware radio note: 433 MHz operation must comply with local
  regulations (e.g. 433/869 MHz in the EU). Pick antennas/power accordingly.
