# Awtrix3 → HUB75 Port

Fork of [Blueforcer/awtrix3](https://github.com/Blueforcer/awtrix3), adapted for a custom ESP32 + HUB75 P2.5 64×32 build.

## Hardware

| Komponente | Detail |
|---|---|
| Controller | ESP32 DevKit V1, 30 Pins, CH340C |
| Panel | HUB75 P2.5 64×32, 1/16 Scan, ICN2037 |
| Sensor | BME280 (I2C SDA=21, SCL=22) |
| Helligkeitssensor | GL5528 LDR (GPIO 34, 10k Pulldown) |
| Netzteil | MeanWell GST25E05-P1J 5V/4A |

## Pinbelegung HUB75 → ESP32

| Signal | GPIO |
|---|---|
| R1 | 25 |
| G1 | 26 |
| B1 | 27 |
| R2 | 14 |
| G2 | 12 |
| B2 | 13 |
| A | 23 |
| B | 19 |
| C | 5 |
| D | 17 |
| CLK | 16 |
| LAT | 4 |
| OE | 15 |

## Build

```bash
pio run -e hub75
pio run -e hub75 -t upload
```

## Dokumentation

Alle technischen Entscheidungen, Pin-Belegungen und Build-Details: [HUB75_PORT_PLAN.md](HUB75_PORT_PLAN.md)

## Lizenz

CC BY-NC-SA 4.0 — siehe [LICENSE.md](LICENSE.md).
Original-Firmware: Copyright (C) 2024 Stephan Mühl (Blueforcer).
