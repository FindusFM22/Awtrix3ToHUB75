# HUB75-Port für AWTRIX3 — Umsetzungsstand

**Status:** Code-Änderungen abgeschlossen inkl. Adversarial-Review-Fixes. **Noch nicht compile-getestet.**
**Zielhardware:** ESP32 DevKit V1 + HUB75 P2.5 64×32 + BME280 (I2C 21/22) + LDR (GPIO 34, 10k Pulldown).
**Was fehlt am Gerät:** Keine Buttons, kein Buzzer, kein DFPlayer, kein Akku.

---

## Panel-Datenblatt (aus AliExpress-Bestellung XuYang LedDisplay)

| Spec | Wert |
|------|------|
| Pixel-Pitch | 2,5 mm |
| Auflösung | 64 × 32 Pixel |
| Modulgröße | 160 × 80 mm |
| **Scan-Modus** | **1/16 Scan** — E-Pin nicht benötigt |
| **Treiber-IC** | **ICN2037** — Standard Shift-Register, kein PWM-Latch |
| Betriebsspannung | 5V DC |
| Max. Stromaufnahme | 3,2–3,5 A |
| Bildwiederholrate | > 600 Hz |
| Pixel-Konfiguration | SMD2121 3-in-1 |

**Netzteil-Match:** MeanWell GST25E05-P1J (4A) ist mit ~13% Reserve korrekt dimensioniert.

**USB-Betrieb NICHT möglich** für Vollbetrieb — bei mehr als ~30 gleichzeitig leuchtenden Pixeln bricht die USB-Spannung ein und der ESP32 rebootet. Für Testzwecke mit wenigen Pixeln bei niedriger Helligkeit funktioniert USB.

---

## Physische HUB75-Steckerbelegung

Standard-Layout (aus Datenblatt/Panel-Bild):

```
Pin 1  RED0 (R1)      Pin 16 GREEN0 (G1)
Pin 2  BLUE0 (B1)     Pin 15 GND
Pin 3  RED1 (R2)      Pin 14 GREEN1 (G2)
Pin 4  BLUE1 (B2)     Pin 13 NC
Pin 5  A              Pin 12 B
Pin 6  C              Pin 11 D
Pin 7  CLK            Pin 10 LATCH
Pin 8  /OE            Pin 9  GND
```

**GND-Pins:** Pin 9 und Pin 15 (beide zusammen auf GND-Schiene).
**Pin 13 (NC):** Nicht angeschlossen — Wannenstecker-Position frei lassen. Assessment: bei 1/16-Scan-Panels typischerweise wirklich NC, aber ohne Multimeter-Test keine 100%-Garantie für dieses Modul.

---

## GPIO-Belegung — Signal → ESP32

| Signal | HUB75 Pin | ESP32 GPIO |
|--------|-----------|------------|
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
| E | — | -1 (nicht belegt, 1/16-Scan) |
| CLK | 7 | 16 |
| LAT | 10 | 4 |
| /OE | 8 | 15 |
| GND | 9, 15 | GND-Schiene |

**Sensoren:**
- BME280: SDA→21, SCL→22, VCC→5V, GND→GND
- LDR: ein Bein an 3,3V, anderes an GPIO 34 + 10kΩ nach GND

---

## Strategie

1. **Capability-Macros statt Build-Env-Macros.** Eine zentrale [src/HardwareConfig.h](src/HardwareConfig.h) entscheidet anhand von `-DULANZI` / `-Dawtrix2_upgrade` / `-DHUB75`, welche der Capability-Flags `HAS_BUTTONS / HAS_BUZZER / HAS_DFPLAYER / HAS_BATTERY / DISPLAY_HUB75 / DISPLAY_WS2812` gesetzt werden. Source-Dateien guarden nur noch gegen die Capability-Flags.
2. **leds[256] CRGB-Buffer bleibt Shadow-Buffer.** Alle App-Calls, Effects, GIFs, Overlays schreiben weiter in den 32×8-Logical-Buffer. Erst am Frame-Ende wird mit `presentFrame()` (WS2812: `FastLED.show()`) bzw. mit `DisplayManager.blitToPanel()` (HUB75: zentriert auf 64×32 mit Offset 16/12) gepusht.
3. **FastLED + FastLED_NeoMatrix bleiben drin**, auch im HUB75-Build — sie betreiben den Shadow-Buffer (CRGB-Helper, NeoMatrix XY-Tile-Mapping). `FastLED.addLeds` wird im HUB75-Build aber nicht aufgerufen, GPIO 32 bleibt frei.

---

## Phase A — Capability-Macro-Refactor

### Neu

- [src/HardwareConfig.h](src/HardwareConfig.h) — entscheidet alle `HAS_*` und `DISPLAY_*` Flags je nach Build-Env. Wird transitiv über [src/Globals.h](src/Globals.h) und [src/PeripheryManager.h](src/PeripheryManager.h) eingebunden.

### Geändert

| Datei | Was |
|---|---|
| [src/Globals.h](src/Globals.h) | `#include "HardwareConfig.h"` — globale Sichtbarkeit |
| [src/PeripheryManager.h](src/PeripheryManager.h) | `EasyButton`-Member nur unter `HAS_BUTTONS`; `BatReadings`-Array nur unter `HAS_BATTERY` |
| [src/PeripheryManager.cpp](src/PeripheryManager.cpp) | Pin-Defines (DFPLAYER_RX/TX, BUZZER_PIN, RESET_PIN, BUTTON_*_PIN, BATTERY_PIN) jeweils in `HAS_*`-Guard; HUB75-Branch im Pin-Block mit LDR=34 + I2C 21/22; globale Konstruktoren (`SoftwareSerial`, `DFMiniMp3`, `MelodyPlayer`, 4× `EasyButton`) in passende Guards; alle Button-Callbacks `HAS_BUTTONS`; alle Sound-Funktionen (`playBootSound`, `stopSound`, `setVolume`, `playRTTTLString`, `playFromFile`, `isPlaying`, `r2d2`) in `HAS_BUZZER`/`HAS_DFPLAYER`; `setup()` Button-/DFPlayer-Init geguardet, neu `analogReadResolution(10)` damit der LDR-Mapping-Code gegen 1023 weiterhin korrekt funktioniert; `tick()` Button-Block + Battery-ADC-Block geguardet |
| [src/Apps.h](src/Apps.h), [src/Apps.cpp](src/Apps.cpp) | `BatApp` Deklaration + Implementierung `#ifdef HAS_BATTERY` |
| [src/DisplayManager.cpp](src/DisplayManager.cpp) | Battery-App-Registrierung und JSON-Stats auf `HAS_BATTERY` migriert |
| [src/MQTTManager.cpp](src/MQTTManager.cpp) | Battery-HASensor-Pointer (Z.24) + Init auf `HAS_BATTERY`; HA-Button-Discovery auf `HAS_BUTTONS`; `sendButton`-Body komplett in `HAS_BUTTONS` |
| [src/MenuManager.cpp](src/MenuManager.cpp) | Battery-Menü-Toggle-Cases auf `HAS_BATTERY` |
| [src/Globals.cpp](src/Globals.cpp) | `BAT_COLOR`/`SHOW_BAT` Load/Save auf `HAS_BATTERY`; `BATTERY_PERCENT`/`BATTERY_RAW` als unconditional Storage (matched extern-Deklarationen in Globals.h); `LittleFS.mkdir("/MELODIES")` auf `HAS_BUZZER` |

### Was Capability-Migration NICHT angefasst hat (bleibt funktional)

- `ServerManager.cpp` `/api/r2d2`-Endpoint und `/api/sound`-Endpoint: rufen `PeripheryManager.*` auf. Die Methoden sind unter HAS_BUZZER/HAS_DFPLAYER no-op — HTTP gibt 200 OK ohne Wirkung. Phantom-Endpoints, aber kein Crash.
- `Overlays.cpp` Notifications mit `sound`/`rtttl`-Feldern: identisch — no-op silent.
- `UpdateManager.cpp` URL-Auswahl: bleibt `#ifdef ULANZI/#elifdef awtrix2_upgrade` — HUB75-Build fällt in den `awtrix2_upgrade`-OTA-Pfad. **Risiko:** OTA holt sich dann awtrix2-Binary. Workaround: OTA per Web-UI manuell ausführen, oder UpdateManager.cpp:13 um HUB75-Variante erweitern (out-of-scope dieser Iteration).
- `MenuManager`: bleibt instanziiert. Da Buttons fehlen, wird `inMenu` nie auf true gesetzt → effektiv tot. Kein Crash, aber statische Allokation.

---

## Phase B — HUB75 Display-Backend

### [platformio.ini](platformio.ini)

Neues Env `[env:hub75]` hinzugefügt, basiert auf `esp32dev`, `-DHUB75`, dependency `mrcodetastic/ESP32 HUB75 LED MATRIX PANEL DMA Display@^3.0.11`.

### [src/main.cpp](src/main.cpp)

`pinMode(15, OUTPUT); digitalWrite(15, LOW);` — die unconditional GPIO15-Sequenz vor `setup()` ist jetzt `#ifndef DISPLAY_HUB75` geguardet. GPIO 15 = HUB75 OE darf nicht extern auf LOW gezogen werden vor der DMA-Init.

### [src/DisplayManager.h](src/DisplayManager.h)

- `#include "HardwareConfig.h"` ergänzt
- neue Methode `void blitToPanel();` unter `#ifdef DISPLAY_HUB75`

### [src/DisplayManager.cpp](src/DisplayManager.cpp)

- HUB75-Header/Macros (PANEL_W=64, PANEL_H=32, dma_display-Pointer). HUB75_OFFSET_X/Y bleiben nur noch als Referenz-Konstanten; die tatsächlichen Offsets werden pro Frame aus `matrix->width()`/`height()` berechnet, damit `ROTATE_SCREEN=true` korrekt gerendert wird.
- `static inline void presentFrame()` nach den globalen Deklarationen — zentrale Frame-Push-Funktion (WS2812: `matrix->show()`, HUB75: `blitToPanel()`)
- Alle 8 internen `matrix->show()`-Aufrufe in DisplayManager.cpp → `presentFrame()`
- `setup()`: HUB75-Branch mit `HUB75_I2S_CFG` (alle 13 Pins explizit, E=-1), `MatrixPanel_I2S_DMA::begin()`, `clearScreen()`, initiale Brightness auf 0 (Panel blank bis `main.cpp:121` finalisiert). Im HUB75-Build wird `setMatrixLayout` NICHT aufgerufen — würde den `matrix`-Pointer durch `delete + new` nochmal austauschen, ohne dass es einen Effekt aufs HUB75-Panel hat
- `setBrightness()`: ruft jetzt `matrix->setBrightness` (für Shadow-Buffer/FastLED-Pfad) UND, wenn HUB75, `dma_display->setBrightness8(static_cast<uint8_t>(bri))` + `clearScreen()` bei bri==0 (HUB75-BCM blankt nicht von alleine)
- `blitToPanel()`: nach `gammaCorrection()` eingefügt. **Liest `leds[matrix->XY(x,y)]`** — wichtige Korrektur gegenüber dem ursprünglichen Plan: weil FastLED_NeoMatrix den Buffer als 4 Tiles à 8×8 indiziert, ist `leds[]` nicht row-major sondern Wire-Order. **Dynamische Grenzen `matrix->width()` und `matrix->height()`** + entsprechende Offsets, damit ROTATE_SCREEN funktioniert. Software-COLOR_CORRECTION + COLOR_TEMPERATURE inline angewendet (FastLED.setCorrection wirkungslos ohne addLeds)
- `setMatrixLayout(layout)` `default:`-Case: Sicherheits-Fallback eingebaut — `default` allokiert nun matrix neu (Layout 1) statt einen baumelnden Pointer in `ui = new MatrixDisplayUi(matrix)` zu schicken

### [src/MatrixDisplayUi.cpp](src/MatrixDisplayUi.cpp)

Z.253 `this->matrix->show()` → `#ifdef DISPLAY_HUB75 DisplayManager.blitToPanel() #else this->matrix->show() #endif`. Alle anderen `matrix->XY(i,j)`-Aufrufe **bleiben unverändert** — sie greifen direkt auf den Shadow-Buffer mit Wire-Order zu, und das passt zu wie alle Apps in den Buffer schreiben (über matrix->drawPixel, das intern XY() durchläuft). Eine Ersetzung zu row-major (`i + j * 32`) wie im ursprünglichen Plan vorgeschlagen wäre falsch und hätte Transitions kaputtgemacht.

---

## HUB75 Library-Konfiguration

In `DisplayManager.cpp::setup()` unter `#ifdef DISPLAY_HUB75`:

```cpp
HUB75_I2S_CFG::i2s_pins pins = {
    /*R1*/ 25, /*G1*/ 26, /*B1*/ 27,
    /*R2*/ 14, /*G2*/ 12, /*B2*/ 13,
    /*A*/ 23,  /*B*/ 19,  /*C*/ 5,
    /*D*/ 17,  /*E*/ -1,
    /*LAT*/ 4, /*OE*/ 15, /*CLK*/ 16
};
HUB75_I2S_CFG mxconfig(64, 32, 1, pins);
mxconfig.clkphase = false;
mxconfig.driver = HUB75_I2S_CFG::SHIFTREG;  // ICN2037 = Shift-Register-Standard
```

**Panel-spezifische Justage bei Fehlanzeige:**
- Geisterbilder / falsche Zeilen: `mxconfig.clkphase = true`
- Falsche Farben in Zeilen: driver-Typ probieren (`ICN2038S`, `FM6124`, `FM6126A`, `MBI5124`)
- Für dieses Panel (ICN2037): `SHIFTREG` sollte funktionieren

---

## Adversarial-Review-Ergebnis (durchlaufen, Fixes eingearbeitet)

### Behoben

- **B1** — `blitToPanel()` mit hartcodierter 32×8-Iteration versagte bei `ROTATE_SCREEN=true`. Jetzt `matrix->width()/height()` + dynamische Offsets pro Frame.
- **R1** — `HASensor *battery`-Pointer war `#ifndef awtrix2_upgrade` → jetzt `#ifdef HAS_BATTERY`.
- **R2** — MenuManager Battery-Toggle-Cases jetzt `HAS_BATTERY`.
- **R3** — Globals.cpp `BATTERY_PERCENT`/`BATTERY_RAW` aus dem awtrix2_upgrade-`#else` rausgezogen (matched extern-Deklarationen in Globals.h).
- **R4** — Duplicate `#define BUZZER_PIN 5` im ESP32_S3-Branch entfernt.
- **R5** — Duplicate `dfmp3.begin()` unter awtrix2_upgrade in setup() entfernt (HAS_DFPLAYER-Block deckt es früher ab).
- **R7** — `LittleFS.mkdir("/MELODIES")` auf `HAS_BUZZER`.
- **R11** — Redundanten frühen `dma_display->setBrightness8(BRIGHTNESS)` in DisplayManager::setup() auf 0 gesetzt. main.cpp:121 finalisiert die Brightness.
- **R13** — Experimentelle Konstanten (`buzzerPin`, `baudRate`, `message`) entfernt bzw. inline in r2d2().
- **R14** — `dma_display->setBrightness8` mit `static_cast<uint8_t>`.

### Nicht angefasst (bewusst)

- **R6** (Reset-Button-Long-Press unter `#ifdef ULANZI`) — nur relevant für hypothetische Non-Ulanzi-Builds mit HAS_BUTTONS. Für dich (HUB75, HAS_BUTTONS aus) irrelevant.
- **R8** (Boot-Animation-Race) — vorbestehend, nicht HUB75-spezifisch.
- **R9** (LDR-Lib re-asserts `analogReadResolution`) — funktional korrekt aktuell.
- **R10** (`setBrightness(0)` clears DMA aber nicht leds[]) — theoretisch, nur relevant wenn Code danach re-blittet ohne Content-Änderung.
- **R12** (Heap-Budget) — brauche echten Flash zum Messen.
- **R15** (`CRGB` truthiness) — durch `haveCorr`/`haveTemp`-Bool eleganter gelöst.
- **R16** (`AUTO_BRIGHTNESS` C++ vs NVS default mismatch) — vorbestehend, nicht HUB75-spezifisch.
- **B2** (AP-MODE-Screen bei Wi-Fi-Fail) — vorbestehend, nicht HUB75-Blocker.

**Verdict des Adversarial Reviews:** Kein Blocker. Build sollte kompilieren und Panel sollte booten.

---

## Verifikation noch nötig

### Compile-Test
```bash
cd /Users/I766285/Documents/Privat/awtrix3
pio run -e hub75
```

Erwartete Knackpunkte:
- `ESP32-HUB75-MatrixPanel-I2S-DMA` Library-Name: `mrcodetastic` vs `mrfaptastic` als Maintainer-Namespace — bei Fehler anderen probieren
- `HUB75_I2S_CFG::SHIFTREG` Enum: Lib-Version >= 3.0.0 hat den

### Flash + Boot

- Erster Sanity: bootet das Gerät, oder hängt es in einer DMA-Init-Race?
- Heap-Check: free heap nach `setup()` ausgeben. HUB75-DMA-Buffer (~24 KB bei 8bit color depth) + AWTRIX-Heap (LittleFS, MQTT 8KB, JSON-Docs) auf ESP32-classic eng. Wenn knapp: `mxconfig.setPixelColorDepthBits(6)` setzen (halbiert DMA-Buffer)
- Panel-Anzeige: WLAN-IP scrollt? Apps cyclen? Text lesbar?

### dev.json-Kalibrierung

- `"ldr_on_ground": true` setzen — deine Schaltung (LDR→3V3, Junction→GPIO 34, 10k→GND) → mehr Licht = höhere ADC. Code-Default ist umgekehrt.
- `"matrix"` ignorieren — HUB75-Build umgeht setMatrixLayout sowieso.
- `MIN_BRIGHTNESS` von 2 auf 8 anheben falls Flackern bei Nacht (HUB75-BCM hat unter ~8 eine Totzone).
- Gamma-Kurve `DisplayManager.cpp:2013` empirisch nachjustieren; Ulanzi-Default `logMap(actualBri, 2, 180, 0.535, 2.3, 1.9)` ist auf WS2812 getuned. Vorschlag für HUB75: `logMap(actualBri, 8, 255, 1.0, 2.2, 1.9)`.

---

## Out-of-Scope-Funde (nicht angefasst)

- [src/DisplayManager.cpp:131-143](src/DisplayManager.cpp) `drawBMP()` ruft `matrix->drawRGBBitmap(y, x, ...)` mit vertauschten Argumenten — entweder Bug oder bewusste Transposition. Tritt für deinen Use-Case (BMP-Icons über HTTP) auf, lohnt separaten Blick.
- [src/UpdateManager.cpp:13](src/UpdateManager.cpp) keine `#elif defined(HUB75)`-Variante — HUB75-Build fällt in awtrix2-OTA-URL. Wenn OTA wichtig: dritte Variante einbauen oder im UpdateManager komplett deaktivieren.

---

## Diff-Footprint

- **10 Dateien geändert**, 1 Datei neu (HardwareConfig.h)
- **~50 Code-Stellen** angefasst (HardwareConfig macros, Pin-Defines/Globale, Setup/Tick-Bodies, presentFrame Helper, HUB75-Init, blitToPanel, Brightness, setMatrixLayout default-fix, main.cpp GPIO15, Adversarial-Review-Fixes)
- **Keine Datei komplett ersetzt**