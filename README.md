# Awtrix3 → HUB75 Port

Fork of [Blueforcer/awtrix3](https://github.com/Blueforcer/awtrix3), adapted for a custom ESP32 + HUB75 P2.5 64×32 build without buttons, buzzer, DFPlayer or battery.

## Hardware

| Komponente | Detail |
|---|---|
| Controller | ESP32 DevKit V1, 30 Pins, CH340C |
| Panel | HUB75 P2.5 64×32, 160×80 mm, 1/16 Scan, ICN2037 |
| Sensor | BME280 (I2C SDA=21, SCL=22) |
| Helligkeitssensor | GL5528 LDR (GPIO 34, 10k Pulldown) |
| Netzteil | MeanWell GST25E05-P1J 5V/4A |

## Pinbelegung HUB75 → ESP32

![HUB75 Pin Layout](HUB32pinLayout.png)

| Signal | HUB75 Pin | ESP32 GPIO |
|---|---|---|
| R1 | 1 | 25 |
| G1 | 16 | 26 |
| B1 | 2 | 27 |
| R2 | 3 | 14 |
| G2 | 14 | 12 |
| B2 | 4 | 13 |
| A | 5 | 23 |
| B | 12 | 19 |
| C | 6 | 5 |
| D | 11 | 17 |
| CLK | 7 | 16 |
| LAT | 10 | 4 |
| /OE | 8 | 15 |
| GND | 9, 15 | GND |

Pin 13 = NC, nicht anschließen.

## Build

Voraussetzung: [PlatformIO](https://platformio.org/)

```bash
# Kompilieren
pio run -e hub75

# Flashen
pio run -e hub75 -t upload

# Serial Monitor
pio device monitor -e hub75
```

**Build-Status (geprüft):**
```
RAM:   30.2%  (99 KB / 328 KB)
Flash: 65.7%  (1220 KB / 1856 KB)  — SUCCESS
```

## Erste Inbetriebnahme

1. ESP32 per USB flashen (Panel noch nicht anschließen)
2. WLAN-Zugangsdaten über AWTRIX AP-Mode (`AWTRIX_xxxx`) eintragen
3. USB trennen, Panel anschließen, MeanWell-Netzteil anschließen
4. Strom — Panel zeigt IP-Adresse, dann Web-UI unter `http://<IP>`

**Wichtig:** USB und MeanWell-5V niemals gleichzeitig anlegen.

## Kalibrierung (dev.json)

Nach dem ersten Boot im Web-UI unter Settings oder direkt als JSON:

```json
{
  "ldr_on_ground": true
}
```

LDR-Polung dieser Schaltung (3V3 → LDR → GPIO34 + 10k → GND) ist umgekehrt zum Ulanzi-Default.

## Dokumentation

Alle technischen Entscheidungen, Capability-Macros und Port-Details: [HUB75_PORT_PLAN.md](HUB75_PORT_PLAN.md)

## Lizenz

CC BY-NC-SA 4.0 — siehe [LICENSE.md](LICENSE.md).
Original-Firmware: Copyright (C) 2024 Stephan Mühl (Blueforcer).
