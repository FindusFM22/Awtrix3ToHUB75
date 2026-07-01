# Plan: Außentemperatur von wttr.in (direkt auf dem ESP)

**Ziel:** Neue native App `OutdoorApp` die alle 10 Minuten `wttr.in` abfragt und Außentemperatur in der App-Rotation anzeigt. Kein externes Gerät, kein MQTT nötig.

**Status:** Geplant — noch nicht umgesetzt.

---

## Schritt 1 — Globals (`Globals.h` / `Globals.cpp`)

### Globals.h — neue extern-Deklarationen

```cpp
extern float OUTDOOR_TEMP;       // aktueller Wert, 0.0 als Default
extern bool OUTDOOR_TEMP_VALID;  // false bis erster Abruf geklappt hat
extern String WEATHER_LOCATION;  // z.B. "Karlsruhe", aus Settings ladbar
extern bool SHOW_OUTDOOR;        // App an/aus, wie SHOW_TEMP
```

### Globals.cpp — Definitionen + Load/Save

```cpp
// Definitionen (bei den anderen Globals)
float OUTDOOR_TEMP = 0.0;
bool OUTDOOR_TEMP_VALID = false;
String WEATHER_LOCATION = "Karlsruhe";
bool SHOW_OUTDOOR = true;

// loadSettings():
WEATHER_LOCATION = Settings.getString("WTR_LOC", "Karlsruhe");
SHOW_OUTDOOR = Settings.getBool("SHOW_OUT", true);

// saveSettings():
Settings.putString("WTR_LOC", WEATHER_LOCATION);
Settings.putBool("SHOW_OUT", SHOW_OUTDOOR);
```

---

## Schritt 2 — HTTP-Fetch in `PeripheryManager`

### PeripheryManager.h — neue Methode deklarieren

```cpp
#ifdef HAS_OUTDOOR_WEATHER
void fetchOutdoorTemp();
#endif
```

### PeripheryManager.cpp — Timer + Fetch-Funktion

Neuen Timer in `tick()` (unabhängig vom Sensor-Timer):

```cpp
#ifdef HAS_OUTDOOR_WEATHER
unsigned long currentMillis_Weather = millis();
if (currentMillis_Weather - previousMillis_Weather >= 600000) { // 10 Minuten
    previousMillis_Weather = currentMillis_Weather;
    fetchOutdoorTemp();
}
#endif
```

Fetch-Funktion:

```cpp
#ifdef HAS_OUTDOOR_WEATHER
void PeripheryManager_::fetchOutdoorTemp() {
    if (!ServerManager.isConnected) return;
    HTTPClient http;
    // ?format=%t&m erzwingt Celsius
    http.begin("http://wttr.in/" + WEATHER_LOCATION + "?format=%t&m");
    http.setTimeout(5000); // max 5s warten
    int code = http.GET();
    if (code == 200) {
        String raw = http.getString(); // z.B. "+18°C" oder "-3°C"
        raw.replace("+", "").replace("°C", "").replace("°", "").trim();
        OUTDOOR_TEMP = raw.toFloat();
        OUTDOOR_TEMP_VALID = true;
    }
    http.end();
}
#endif
```

**Neuer Timer-Global** (oben in PeripheryManager.cpp neben den anderen):

```cpp
#ifdef HAS_OUTDOOR_WEATHER
unsigned long previousMillis_Weather = 0;
#endif
```

---

## Schritt 3 — Neue App (`Apps.h` / `Apps.cpp`)

### Apps.h — Prototyp

```cpp
#ifdef HAS_OUTDOOR_WEATHER
void OutdoorApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                int16_t x, int16_t y, GifPlayer *gifPlayer);
#endif
```

### Apps.cpp — Implementierung

Analog zu `TempApp`:

```cpp
#ifdef HAS_OUTDOOR_WEATHER
void OutdoorApp(FastLED_NeoMatrix *matrix, MatrixDisplayUiState *state,
                int16_t x, int16_t y, GifPlayer *gifPlayer) {
    if (notifyFlag) return;
    if (!OUTDOOR_TEMP_VALID) return; // nichts anzeigen bis erster Abruf
    CURRENT_APP = "Outdoor";
    currentCustomApp = "";
    // Icon 2056 = Sonne (eingebaut), oder eigenes Icon über LittleFS
    matrix->drawRGBBitmap(x, y, icon_2056, 8, 8);
    DisplayManager.setCursor(14 + x, 6 + y);
    DisplayManager.matrixPrint(OUTDOOR_TEMP, 1); // eine Dezimalstelle
    DisplayManager.matrixPrint("°");
}
#endif
```

---

## Schritt 4 — App registrieren (`DisplayManager.cpp`)

### In `loadNativeApps()`

```cpp
#ifdef HAS_OUTDOOR_WEATHER
updateApp("Outdoor", OutdoorApp, SHOW_OUTDOOR, 5); // Slot 5
#endif
```

### In `getNativeAppByName()`

```cpp
#ifdef HAS_OUTDOOR_WEATHER
else if (appName == "Outdoor") {
    return std::make_pair("Outdoor", OutdoorApp);
}
#endif
```

---

## Schritt 5 — Settings-API (`DisplayManager.cpp`)

In `setNewSettings()` — analog zu `SHOW_TEMP`:

```cpp
#ifdef HAS_OUTDOOR_WEATHER
SHOW_OUTDOOR = doc.containsKey("SHOW_OUT") ?
    doc["SHOW_OUT"].as<bool>() : SHOW_OUTDOOR;
WEATHER_LOCATION = doc.containsKey("WTR_LOC") ?
    doc["WTR_LOC"].as<String>() : WEATHER_LOCATION;
#endif
```

Damit konfigurierbar per curl:

```bash
curl -X POST http://<IP>/api/settings \
  -H "Content-Type: application/json" \
  -d '{"WTR_LOC": "Berlin", "SHOW_OUT": true}'
```

---

## Schritt 6 — Capability-Guard (`HardwareConfig.h`)

```cpp
#elif defined(HUB75)
  #define DISPLAY_HUB75
  #define HAS_OUTDOOR_WEATHER   // wttr.in fetch, nur im WiFi-Build
```

Alle obigen Stellen sind unter `#ifdef HAS_OUTDOOR_WEATHER` — kein Einfluss auf Ulanzi-Build, kein Einfluss auf awtrix2_upgrade.

---

## Umsetzungsreihenfolge

1. `HardwareConfig.h` — `HAS_OUTDOOR_WEATHER` im HUB75-Block
2. `Globals.h` / `Globals.cpp` — Variablen + Load/Save
3. `PeripheryManager.h` / `.cpp` — Timer + `fetchOutdoorTemp()`
4. Build testen — kompiliert der HTTPClient sauber?
5. `Apps.h` / `Apps.cpp` — `OutdoorApp` anlegen
6. `DisplayManager.cpp` — registrieren + Settings-API
7. Visual-Test: Location falsch → App zeigt nichts → korrigieren → nach 10 min erscheint Wert

---

## Risiken

| Risiko | Absicherung |
|---|---|
| wttr.in offline / langsam | 5s Timeout, `OUTDOOR_TEMP_VALID` bleibt false, App zeigt nichts |
| Kein WiFi beim Boot | `ServerManager.isConnected`-Check vor HTTP-Request |
| Celsius vs Fahrenheit | `?format=%t&m` erzwingt metrisch |
| ESP hängt beim Fetch | `http.setTimeout(5000)` — nach 5s weiter |
| Erster Wert fehlt 10 Min | `OUTDOOR_TEMP_VALID=false` → App übersprungen bis Wert da |
