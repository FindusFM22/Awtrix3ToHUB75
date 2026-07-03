#include <PeripheryManager.h>
#include "Adafruit_SHT31.h"
#include "Adafruit_BME280.h"
#include "Adafruit_BMP280.h"
#include "Adafruit_HTU21DF.h"
#ifdef HAS_DFPLAYER
#include "SoftwareSerial.h"
#include <DFMiniMp3.h>
#endif
#ifdef HAS_BUZZER
#include <MelodyPlayer/melody_player.h>
#include <MelodyPlayer/melody_factory.h>
#endif
#include "Globals.h"
#include "DisplayManager.h"
#include "MQTTManager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <LightDependentResistor.h>
#include <MenuManager.h>
#ifdef HAS_OUTDOOR_WEATHER
#include <HTTPClient.h>
#include <WiFi.h>
#endif
#include <ServerManager.h>
#include <MedianFilterLib.h>
#include <MeanFilterLib.h>
#include <Games/GameManager.h>
#define LEDC_CHANNEL 0
#define LEDC_RESOLUTION 8 // 8 bit resolution
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define MEDIAN_WND 7 // A median filter window size of seven should be enough to filter out most spikes
#define MEAN_WND 7   // After filtering the spikes we don't need many samples anymore for the average

#ifdef HAS_DFPLAYER
#define DFPLAYER_RX 23
#define DFPLAYER_TX 18
#endif
#ifdef HAS_BUZZER
#define BUZZER_PIN 15
#endif
#ifdef HAS_BUTTONS
#define RESET_PIN 13
#endif

#ifdef awtrix2_upgrade
// Pinouts für das WEMOS_D1_MINI32-Environment
#define LDR_PIN A0
#define BUTTON_UP_PIN D0
#define BUTTON_DOWN_PIN D8
#define BUTTON_SELECT_PIN D4

#define I2C_SCL_PIN D1
#define I2C_SDA_PIN D3
#elif ESP32_S3
#define BATTERY_PIN 4
#define LDR_PIN 6
#define BUTTON_UP_PIN 7
#define BUTTON_DOWN_PIN 8
#define BUTTON_SELECT_PIN 10
#define I2C_SCL_PIN 10
#define I2C_SDA_PIN 11
#elif defined(HUB75)
// Custom HUB75 build — only LDR + I2C, no battery/buttons.
// LDR on GPIO 34 (10k pulldown, other leg to 3V3 -> LDR_ON_GROUND=false).
#define LDR_PIN 34
#define I2C_SCL_PIN 22
#define I2C_SDA_PIN 21
#else
// Pinouts für das ULANZI-Environment
#define BATTERY_PIN 34

#define LDR_PIN 35
#define BUTTON_UP_PIN 26
#define BUTTON_DOWN_PIN 14
#define BUTTON_SELECT_PIN 27
#define I2C_SCL_PIN 22
#define I2C_SDA_PIN 21
#endif

Adafruit_BME280 bme280;
Adafruit_BMP280 bmp280;
Adafruit_HTU21DF htu21df;
Adafruit_SHT31 sht31;

#ifdef awtrix2_upgrade
#define USED_PHOTOCELL LightDependentResistor::GL5528
#define PHOTOCELL_SERIES_RESISTOR 1000
#elif defined(HUB75)
// GL5528 mit 10kΩ Pull-Down
#define USED_PHOTOCELL LightDependentResistor::GL5528
#define PHOTOCELL_SERIES_RESISTOR 10000
#else
#define USED_PHOTOCELL LightDependentResistor::GL5516
#define PHOTOCELL_SERIES_RESISTOR 10000
#endif

#ifdef HAS_DFPLAYER
class Mp3Notify
{
};
SoftwareSerial mySoftwareSerial(DFPLAYER_RX, DFPLAYER_TX); // RX, TX
DFMiniMp3<SoftwareSerial, Mp3Notify> dfmp3(mySoftwareSerial);
#endif

#ifdef HAS_BUZZER
MelodyPlayer player(BUZZER_PIN, 1, LOW);
#endif

#ifdef HAS_BUTTONS
EasyButton button_left(BUTTON_UP_PIN);
EasyButton button_right(BUTTON_DOWN_PIN);
EasyButton button_select(BUTTON_SELECT_PIN);
EasyButton button_reset(RESET_PIN);
#endif

LightDependentResistor photocell(LDR_PIN,
                                 PHOTOCELL_SERIES_RESISTOR,
                                 USED_PHOTOCELL,
                                 10,
                                 10);

int readIndex = 0;
int sampleIndex = 0;
unsigned long previousMillis_BatTempHum = 0;
unsigned long previousMillis_LDR = 0;
const unsigned long interval_BatTempHum = 10000;
const unsigned long interval_LDR = 100;
#ifdef HAS_OUTDOOR_WEATHER
// -1 forces first fetch as soon as WiFi is up (rather than waiting 10 min).
unsigned long previousMillis_Weather = 0;
bool weatherFetchDone = false;
const unsigned long interval_Weather = 900000UL; // 15minutes
#endif
int total = 0;
unsigned long startTime;

#ifdef HAS_BATTERY
MedianFilter<uint16_t> medianFilterBatt(MEDIAN_WND);
MeanFilter<uint16_t> meanFilterBatt(MEAN_WND);
#endif
MedianFilter<uint16_t> medianFilterLDR(MEDIAN_WND);
MeanFilter<uint16_t> meanFilterLDR(MEAN_WND);

float brightnessPercent = 0.0;

PeripheryManager_::PeripheryManager_()
{
#ifdef HAS_BUTTONS
    this->buttonL = &button_left;
    this->buttonR = &button_right;
    this->buttonS = &button_select;
    this->buttonRST = &button_reset;
#endif
}

// The getter for the instantiated singleton instance
PeripheryManager_ &PeripheryManager_::getInstance()
{
    static PeripheryManager_ instance;
    return instance;
}

// Initialize the global shared instance
PeripheryManager_ &PeripheryManager = PeripheryManager.getInstance();

void left_button_pressed()
{
#ifdef HAS_BUTTONS
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        DisplayManager.leftButton();
        MenuManager.leftButton();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Left button clicked"));
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Left button clicked but blocked"));
    }
#endif
}

void right_button_pressed()
{
#ifdef HAS_BUTTONS
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        DisplayManager.rightButton();
        MenuManager.rightButton();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Right button clicked"));
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Right button clicked but blocked"));
    }
#endif
}

void select_button_pressed()
{
#ifdef HAS_BUTTONS
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        DisplayManager.selectButton();
        MenuManager.selectButton();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Select button clicked"));
    }
    else
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Select button clicked but blocked"));
    }
#endif
}

void reset_button_pressed_long()
{
#ifdef HAS_BUTTONS
    ServerManager.erase();
    ESP.restart();
#endif
}

void select_button_pressed_long()
{
#ifdef HAS_BUTTONS
    if (DFPLAYER_ACTIVE)
        PeripheryManager.playFromFile(DFMINI_MP3_CLICK);
    if (AP_MODE)
    {
        ++MATRIX_LAYOUT;
        if (MATRIX_LAYOUT < 0)
            MATRIX_LAYOUT = 2;
        saveSettings();
        ESP.restart();
    }
    else if (!BLOCK_NAVIGATION)
    {
        MenuManager.selectButtonLong();
        DisplayManager.selectButtonLong();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Select button pressed long"));
    }
#endif
}

void select_button_double()
{
#ifdef HAS_BUTTONS
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Select button double pressed"));
    if (!BLOCK_NAVIGATION)
    {
        if (DFPLAYER_ACTIVE)
            PeripheryManager.playFromFile(DFMINI_MP3_CLICK);

        if (MATRIX_OFF)
        {
            DisplayManager.setPower(true);
        }
        else
        {
            DisplayManager.setPower(false);
        }
    }
#endif
}

void PeripheryManager_::playBootSound()
{
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Playing bootsound"));
    if (!SOUND_ACTIVE)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Sound output disabled"));
        return;
    }

    if (BOOT_SOUND == "")
    {
#ifdef HAS_DFPLAYER
        if (DFPLAYER_ACTIVE)
        {
            playFromFile(DFMINI_MP3_BOOT);
            return;
        }
#endif
#ifdef HAS_BUZZER
        const int nNotes = 6;
        String notes[nNotes] = {"E5", "C5", "G4", "E4", "G4", "C5"};
        const int timeUnit = 150;
        Melody melody = MelodyFactory.load("Bootsound", timeUnit, notes, nNotes);
        player.playAsync(melody);
#endif
    }
    else
    {
        playFromFile(BOOT_SOUND);
    }
}

void PeripheryManager_::stopSound()
{
#ifdef HAS_DFPLAYER
    if (DFPLAYER_ACTIVE)
    {
        dfmp3.stopAdvertisement();
        delay(50);
        dfmp3.stop();
        return;
    }
#endif
#ifdef HAS_BUZZER
    player.stop();
#endif
}

void PeripheryManager_::setVolume(uint8_t vol)
{
#ifdef HAS_DFPLAYER
    if (DFPLAYER_ACTIVE)
    {
        uint8_t curVolume = dfmp3.getVolume(); // need to read volume in order to work. Donno why! :(
        dfmp3.setVolume(vol);
        delay(50);
        return;
    }
#endif
#ifdef HAS_BUZZER
    int scaledVol = (vol * 255) / 30;
    player.setVolume(scaledVol);
#endif
}

bool PeripheryManager_::parseSound(const char *json)
{
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        return playFromFile(String(json));
    }
    if (doc.containsKey("sound"))
    {
        return playFromFile(doc["sound"].as<String>());
    }
    return false;
}

const char *PeripheryManager_::playRTTTLString(String rtttl)
{
#ifdef HAS_BUZZER
    if (!DFPLAYER_ACTIVE && SOUND_ACTIVE)
    {
        static char melodyName[64];
        Melody melody = MelodyFactory.loadRtttlString(rtttl.c_str());
        player.playAsync(melody);
        strncpy(melodyName, melody.getTitle().c_str(), sizeof(melodyName));
        melodyName[sizeof(melodyName) - 1] = '\0';
        return melodyName;
    }
#endif
    return nullptr; // RTTTL not supported with DFPlayer / no buzzer
}

const char *PeripheryManager_::playFromFile(String file)
{
    if (!SOUND_ACTIVE)
        return "";

#ifdef HAS_DFPLAYER
    if (DFPLAYER_ACTIVE)
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Playing MP3 file"));
        dfmp3.stop();
        delay(50);
        dfmp3.playMp3FolderTrack(file.toInt());

        return file.c_str();
    }
#endif
#ifdef HAS_BUZZER
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Playing RTTTL sound file"));
    if (LittleFS.exists("/MELODIES/" + String(file) + ".txt"))
    {
        static char melodyName[64];
        Melody melody = MelodyFactory.loadRtttlFile("/MELODIES/" + String(file) + ".txt");
        player.playAsync(melody);
        strncpy(melodyName, melody.getTitle().c_str(), sizeof(melodyName));
        melodyName[sizeof(melodyName) - 1] = '\0';
        return melodyName;
    }
#endif
    return NULL;
}

bool PeripheryManager_::isPlaying()
{
#ifdef HAS_DFPLAYER
    if (DFPLAYER_ACTIVE)
    {
        if ((dfmp3.getStatus() & 0xff) == 0x01) // 0x01 = DfMp3_StatusState_Playing
            return true;
        else
            return false;
    }
#endif
#ifdef HAS_BUZZER
    return player.isPlaying();
#else
    return false;
#endif
}

void PeripheryManager_::setup()
{
    if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Setup periphery"));
    startTime = millis();

    // ESP32 ADC defaults to 12-bit (0..4095). The Ulanzi-era code in tick()
    // normalises against 1023, so force the resolution down to keep the LDR
    // mapping working without rewriting every consumer.
    analogReadResolution(10);

    pinMode(LDR_PIN, INPUT);
#ifdef HAS_BUTTONS
    pinMode(RESET_PIN, INPUT);
#endif
#ifdef HAS_DFPLAYER
    if (DFPLAYER_ACTIVE)
    {
        dfmp3.begin();
        delay(100);
        setVolume(SOUND_VOLUME);
    }
#endif
#ifdef HAS_BUTTONS
    button_left.begin();
    button_right.begin();
    button_select.begin();
    button_reset.begin();

    if ((ROTATE_SCREEN && !SWAP_BUTTONS) || (!ROTATE_SCREEN && SWAP_BUTTONS))
    {
        Serial.println("Button rotation");
        button_left.onPressed(right_button_pressed);
        button_right.onPressed(left_button_pressed);
    }
    else
    {
        button_left.onPressed(left_button_pressed);
        button_right.onPressed(right_button_pressed);
    }

    button_select.onPressed(select_button_pressed);
    button_select.onPressedFor(1000, select_button_pressed_long);
    button_select.onSequence(2, 300, select_button_double);

#ifdef ULANZI
    button_reset.onPressedFor(5000, reset_button_pressed_long);
#endif
#endif // HAS_BUTTONS

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    if (bme280.begin(BME280_ADDRESS) || bme280.begin(BME280_ADDRESS_ALTERNATE))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("BME280 sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_BME280;
    }
    else if (bmp280.begin(BMP280_ADDRESS) || bmp280.begin(BMP280_ADDRESS_ALT))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("BMP280 sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_BMP280;
    }
    else if (htu21df.begin())
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("HTU21DF sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_HTU21DF;
    }
    else if (sht31.begin(0x44))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("SHT31 sensor detected"));
        TEMP_SENSOR_TYPE = TEMP_SENSOR_TYPE_SHT31;
    }

    // (Historical duplicate dfmp3.begin() removed — the HAS_DFPLAYER-guarded
    // block earlier in setup() already covers the awtrix2_upgrade case.)
    if (!LDR_ON_GROUND)
        photocell.setPhotocellPositionOnGround(false);
}

#ifdef HAS_OUTDOOR_WEATHER
// Map the "%C" text description wttr.in appends to a short icon slug. The
// full text is compared case-insensitively as a substring so partial matches
// like "Partly cloudy" or "Light rain shower" resolve to the closest slug.
// Order matters: check more specific terms (thunder, storm, snow, rain, fog)
// before the broader "cloud"/"clear" catch-alls.
static String weatherTextToIconSlug(const String &raw)
{
    String s = raw;
    s.toLowerCase();
    if (s.indexOf("thunder") >= 0 || s.indexOf("storm") >= 0) return "storm";
    // Sleet must be checked before generic "snow"/"rain" — otherwise "rain and
    // snow" or "sleet" would resolve to plain snow or rain.
    if (s.indexOf("sleet") >= 0
        || (s.indexOf("rain") >= 0 && s.indexOf("snow") >= 0))
        return "sleet";
    if (s.indexOf("snow") >= 0 || s.indexOf("blizzard") >= 0) return "snow";
    if (s.indexOf("rain") >= 0 || s.indexOf("shower") >= 0 || s.indexOf("drizzle") >= 0) return "rain";
    if (s.indexOf("fog") >= 0 || s.indexOf("mist") >= 0 || s.indexOf("haze") >= 0 || s.indexOf("smog") >= 0) return "fog";
    if (s.indexOf("partly") >= 0) return "partlycloudy";
    if (s.indexOf("cloud") >= 0 || s.indexOf("overcast") >= 0) return "cloudy";
    if (s.indexOf("clear") >= 0 || s.indexOf("sunny") >= 0 || s.indexOf("fair") >= 0) return "clear";
    return "unknown";
}

// Fetch outdoor temperature + weather condition from wttr.in.
// Uses the "%t|%C" format so we get "+18°C|Partly cloudy" style responses;
// the pipe avoids ambiguity when the condition itself contains spaces.
// Called every ~10 minutes from tick(); no-op unless WiFi is up.
void PeripheryManager_::fetchOutdoorTemp()
{
    if (WiFi.status() != WL_CONNECTED)
        return;

    HTTPClient http;
    char url[96];
    // 4 decimal places is well below wttr.in's practical location resolution
    // and keeps the URL comfortably under any header size limits.
    // Format: "+18°C|Partly cloudy|3|72" — %t (temp) | %C (condition) | %u (UV) | %h (humidity)
    snprintf(url, sizeof(url), "http://wttr.in/%.4f,%.4f?format=%%t|%%C|%%u|%%h&m",
             OUTDOOR_LAT, OUTDOOR_LON);
    http.begin(url);
    http.setTimeout(5000);
    // Some CDNs return HTML if we advertise a browser; wttr.in respects curl-ish agents.
    http.setUserAgent("curl/8.0 awtrix3");

    const int code = http.GET();
    if (code == HTTP_CODE_OK)
    {
        String body = http.getString();
        body.trim();
        // Split at "|" delimiters.
        int sep1 = body.indexOf('|');
        int sep2 = (sep1 >= 0) ? body.indexOf('|', sep1 + 1) : -1;
        int sep3 = (sep2 >= 0) ? body.indexOf('|', sep2 + 1) : -1;
        String tPart = (sep1 > 0) ? body.substring(0, sep1) : body;
        String cPart = (sep1 > 0 && sep2 > 0) ? body.substring(sep1 + 1, sep2)
                     : (sep1 > 0) ? body.substring(sep1 + 1) : "";
        String uPart = (sep2 > 0 && sep3 > 0) ? body.substring(sep2 + 1, sep3)
                     : (sep2 > 0) ? body.substring(sep2 + 1) : "";
        String hPart = (sep3 > 0) ? body.substring(sep3 + 1) : "";

        tPart.replace("+", "");
        // wttr.in returns "°C" (UTF-8). Strip everything from the ° onward.
        int deg = tPart.indexOf("\xC2\xB0");
        if (deg < 0) deg = tPart.indexOf("°");
        if (deg > 0) tPart = tPart.substring(0, deg);
        tPart.trim();
        uPart.trim();
        hPart.trim();

        float parsed = tPart.toFloat();
        // toFloat returns 0.0 on failure — accept 0 only if the string was "0" or "0.0"
        // (rare but possible at freezing). Guard the else branch instead of hard-erroring.
        if (parsed != 0.0f || tPart == "0" || tPart == "0.0" || tPart == "-0" || tPart == "-0.0")
        {
            OUTDOOR_TEMP = parsed;
            OUTDOOR_ICON = weatherTextToIconSlug(cPart);
            OUTDOOR_TEMP_VALID = true;
            // UV index: wttr.in returns an integer 0..11+; empty or non-numeric → -1
            if (uPart.length() > 0 && isdigit((unsigned char)uPart[0]))
                OUTDOOR_UV = uPart.toInt();
            else
                OUTDOOR_UV = -1;
            // Humidity: wttr.in returns "72%" — strip the % sign
            hPart.replace("%", "");
            if (hPart.length() > 0 && isdigit((unsigned char)hPart[0]))
                OUTDOOR_HUM = hPart.toInt();
            else
                OUTDOOR_HUM = -1;
        }
    }
    http.end();
}
#endif // HAS_OUTDOOR_WEATHER

void PeripheryManager_::tick()
{
#ifdef HAS_BUTTONS
    if (!MenuManager.inMenu)
    {
        if (ROTATE_SCREEN)
        {
            MQTTManager.sendButton(2, button_left.read());
            ServerManager.sendButton(2, button_left.read());
            MQTTManager.sendButton(0, button_right.read());
            ServerManager.sendButton(0, button_right.read());
        }
        else
        {
            MQTTManager.sendButton(0, button_left.read());
            MQTTManager.sendButton(2, button_right.read());
            ServerManager.sendButton(0, button_left.read());
            ServerManager.sendButton(2, button_right.read());
        }

        MQTTManager.sendButton(1, button_select.read());
        ServerManager.sendButton(1, button_select.read());
    }
    else
    {
        button_left.read();
        button_select.read();
        button_right.read();
    }

    button_reset.read();
#endif // HAS_BUTTONS

    unsigned long currentMillis_BatTempHum = millis();
    if (currentMillis_BatTempHum - previousMillis_BatTempHum >= interval_BatTempHum)
    {
        previousMillis_BatTempHum = currentMillis_BatTempHum;
#ifdef HAS_BATTERY
        uint16_t ADCVALUE = analogRead(BATTERY_PIN);
        // Discard values that are totally out of range, especially the first value read after a reboot.
        // Meaningful values for an Ulanzi clock are in the range 400..700
        if ((ADCVALUE > 100) && (ADCVALUE < 1000))
        {
            // Send ADC values through median filter to get rid of the remaining spikes and then calculate the average
            BATTERY_RAW = meanFilterBatt.AddValue(medianFilterBatt.AddValue(ADCVALUE));
            BATTERY_PERCENT = max(min((int)map(BATTERY_RAW, MIN_BATTERY, MAX_BATTERY, 0, 100), 100), 0);
            SENSORS_STABLE = true;
        }
#else
        SENSORS_STABLE = true;
#endif
        if (SENSOR_READING)
        {
            switch (TEMP_SENSOR_TYPE)
            {
            case TEMP_SENSOR_TYPE_BME280:
                CURRENT_TEMP = bme280.readTemperature();
                CURRENT_HUM = bme280.readHumidity();
                break;
            case TEMP_SENSOR_TYPE_BMP280:
                CURRENT_TEMP = bmp280.readTemperature();
                CURRENT_HUM = 0;
                break;
            case TEMP_SENSOR_TYPE_HTU21DF:
                CURRENT_TEMP = htu21df.readTemperature();
                CURRENT_HUM = htu21df.readHumidity();
                break;
            case TEMP_SENSOR_TYPE_SHT31:
                sht31.readBoth(&CURRENT_TEMP, &CURRENT_HUM);
                break;
            default:
                CURRENT_TEMP = 0;
                CURRENT_HUM = 0;
                break;
            }

            CURRENT_TEMP += TEMP_OFFSET;
            CURRENT_HUM += HUM_OFFSET;
        }
        else
        {
            SENSORS_STABLE = true;
        }
    }

    unsigned long currentMillis_LDR = millis();
    if (currentMillis_LDR - previousMillis_LDR >= interval_LDR)
    {
        previousMillis_LDR = currentMillis_LDR;

        uint16_t LDRVALUE = analogRead(LDR_PIN);
        if (LDR_ON_GROUND)
            LDRVALUE = 1023.0 - LDRVALUE;
        // Send LDR values through median filter to get rid of the remaining spikes and then calculate the average
        LDR_RAW = meanFilterLDR.AddValue(medianFilterLDR.AddValue(LDRVALUE));
        CURRENT_LUX = (roundf(photocell.getSmoothedLux() * 1000) / 1000);
        if (AUTO_BRIGHTNESS && !MATRIX_OFF)
        {
            brightnessPercent = (LDR_RAW * LDR_FACTOR) / 1023.0 * 100.0;
            brightnessPercent = pow(brightnessPercent, LDR_GAMMA) / pow(100.0, LDR_GAMMA - 1);
            BRIGHTNESS = map(brightnessPercent, 0, 100, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
            DisplayManager.setBrightness(BRIGHTNESS);
        }
    }

#ifdef HAS_OUTDOOR_WEATHER
    // First fetch happens as soon as WiFi is up; subsequent fetches every 10 min.
    // weatherFetchDone stays false until the first successful call, so retries
    // happen on every tick until the network is actually ready.
    const unsigned long nowW = millis();
    const bool timeForRefresh = weatherFetchDone
        ? (nowW - previousMillis_Weather >= interval_Weather)
        : true;
    if (timeForRefresh)
    {
        previousMillis_Weather = nowW;
        fetchOutdoorTemp();
        if (OUTDOOR_TEMP_VALID)
            weatherFetchDone = true;
    }
#endif
}

unsigned long long PeripheryManager_::readUptime()
{
    static unsigned long lastTime = 0;
    static unsigned long long totalElapsed = 0;

    unsigned long currentTime = millis();
    if (currentTime < lastTime)
    {
        // millis() overflow
        totalElapsed += 4294967295UL - lastTime + currentTime + 1;
    }
    else
    {
        totalElapsed += currentTime - lastTime;
    }
    lastTime = currentTime;

    unsigned long long uptimeSeconds = totalElapsed / 1000;
    return uptimeSeconds;
}

void PeripheryManager_::r2d2(const char *msg)
{
#ifdef HAS_BUZZER
    // Little R2-D2 impression: each letter maps to a tone; ~60 ms per "phoneme".
    for (int i = 0; msg[i] != '\0'; i++)
    {
        char c = msg[i];
        tone(BUZZER_PIN, (c - 'A' + 1) * 50);
        delay(60);
    }
    noTone(BUZZER_PIN);
#endif
}