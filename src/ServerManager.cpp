#include "ServerManager.h"
#include "Globals.h"
#include <WebServer.h>
#include <esp-fs-webserver.h>
#include "htmls.h"
#include <Update.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <WiFi.h>
#include "DisplayManager.h"
#include "UpdateManager.h"
#include "PeripheryManager.h"
#include "PowerManager.h"
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include "Games/GameManager.h"
#include <EEPROM.h>

WiFiUDP udp;

unsigned int localUdpPort = 4210;
char incomingPacket[256];

// Pufferdefinition
#define BUFFER_SIZE 64
char dataBuffer[BUFFER_SIZE];
int bufferIndex = 0;

// Aktueller verbundener Client
WiFiClient currentClient = WiFiClient();
WebServer server(80);
FSWebServer mws(LittleFS, server);

// Erstelle eine Server-Instanz
WiFiServer TCPserver(8080);

// The getter for the instantiated singleton instance
ServerManager_ &ServerManager_::getInstance()
{
    static ServerManager_ instance;
    return instance;
}

// Initialize the global shared instance
ServerManager_ &ServerManager = ServerManager.getInstance();

void versionHandler()
{
    WebServerClass *webRequest = mws.getRequest();
    webRequest->send(200, F("text/plain"), VERSION);
}

void ServerManager_::erase()
{
    DisplayManager.HSVtext(0, 6, "RESET", true, 0);
    wifi_config_t conf;
    memset(&conf, 0, sizeof(conf)); // Set all the bytes in the structure to 0
    esp_wifi_set_config(WIFI_IF_STA, &conf);
    LittleFS.format();
    delay(200);
    formatSettings();
    delay(200);
}

void saveHandler()
{
    WebServerClass *webRequest = mws.getRequest();
    ServerManager.getInstance().loadSettings();
    webRequest->send(200);
}

void addHandler()
{

    mws.addHandler("/api/power", HTTP_POST, []()
                   { DisplayManager.powerStateParse(mws.webserver->arg("plain").c_str()); mws.webserver->send(200,F("text/plain"),F("OK")); });
    mws.addHandler(
        "/api/sleep", HTTP_POST, []()
        { 
            mws.webserver->send(200,F("text/plain"),F("OK"));
            DisplayManager.setPower(false);
            PowerManager.sleepParser(mws.webserver->arg("plain").c_str()); });
    mws.addHandler("/api/loop", HTTP_GET, []()
                   { mws.webserver->send_P(200, "application/json", DisplayManager.getAppsAsJson().c_str()); });
    mws.addHandler("/api/effects", HTTP_GET, []()
                   { mws.webserver->send_P(200, "application/json", DisplayManager.getEffectNames().c_str()); });
    mws.addHandler("/api/transitions", HTTP_GET, []()
                   { mws.webserver->send_P(200, "application/json", DisplayManager.getTransitionNames().c_str()); });
    mws.addHandler("/api/reboot", HTTP_ANY, []()
                   { mws.webserver->send(200,F("text/plain"),F("OK")); delay(200); ESP.restart(); });
#ifdef DISPLAY_HUB75
    // Debug: paint the full native panel a single colour to verify every
    // pixel is addressable. Bypasses the app renderer and its 32x8 clip.
    // Usage: POST /api/hub75/fill?c=00FF00&d=60000 (hex RRGGBB, hold ms)
    mws.addHandler("/api/hub75/fill", HTTP_ANY, []()
                   {
                       String c = mws.webserver->hasArg("c") ? mws.webserver->arg("c") : "FFFFFF";
                       uint32_t rgb = strtoul(c.c_str(), nullptr, 16);
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75FillTest(rgb, dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: interior fill + 1-pixel border.
    // Usage: POST /api/hub75/border?c=0000FF&b=FFFFFF&d=60000
    mws.addHandler("/api/hub75/border", HTTP_ANY, []()
                   {
                       String c = mws.webserver->hasArg("c") ? mws.webserver->arg("c") : "0000FF";
                       String bC = mws.webserver->hasArg("b") ? mws.webserver->arg("b") : "FFFFFF";
                       uint32_t fill = strtoul(c.c_str(), nullptr, 16);
                       uint32_t border = strtoul(bC.c_str(), nullptr, 16);
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75BorderTest(fill, border, dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: adjust HUB75 latch blanking at runtime to tune ghosting.
    // Higher values (up to library max) blank OE longer around LAT, reducing
    // bleed between rows at a small refresh-rate cost. Usage:
    //   POST /api/hub75/lat?n=4
    mws.addHandler("/api/hub75/lat", HTTP_ANY, []()
                   {
                       int n = mws.webserver->hasArg("n") ? mws.webserver->arg("n").toInt() : 4;
                       uint8_t got = DisplayManager.hub75SetLatBlanking((uint8_t)n);
                       String reply = String("latch_blanking=") + got;
                       mws.webserver->send(200, F("text/plain"), reply);
                   });
    // Debug: 3x3 corner markers (TL=red, TR=green, BL=blue, BR=yellow) on black.
    // Reveals pixel-shift / stretch bugs. Usage: POST /api/hub75/corners?d=60000
    mws.addHandler("/api/hub75/corners", HTTP_ANY, []()
                   {
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75CornerTest(dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: light single pixel. Usage: POST /api/hub75/px?x=0&y=29&c=FFFFFF&d=60000
    mws.addHandler("/api/hub75/px", HTTP_ANY, []()
                   {
                       int x = mws.webserver->hasArg("x") ? mws.webserver->arg("x").toInt() : 0;
                       int y = mws.webserver->hasArg("y") ? mws.webserver->arg("y").toInt() : 0;
                       String c = mws.webserver->hasArg("c") ? mws.webserver->arg("c") : "FFFFFF";
                       uint32_t rgb = strtoul(c.c_str(), nullptr, 16);
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75Pixel(x, y, rgb, dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: column sweep R G B W R G B W ... from right to left.
    // Usage: POST /api/hub75/cols?d=60000
    mws.addHandler("/api/hub75/cols", HTTP_ANY, []()
                   {
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75ColSweep(dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: row sweep R G B W R G B W ... top-down.
    // Usage: POST /api/hub75/rows?d=60000
    mws.addHandler("/api/hub75/rows", HTTP_ANY, []()
                   {
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75RowSweep(dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: 1x1 checkerboard. Usage:
    //   POST /api/hub75/check?a=FF0000&b=00FF00&d=60000
    mws.addHandler("/api/hub75/check", HTTP_ANY, []()
                   {
                       String a = mws.webserver->hasArg("a") ? mws.webserver->arg("a") : "FFFFFF";
                       String b = mws.webserver->hasArg("b") ? mws.webserver->arg("b") : "000000";
                       uint32_t ra = strtoul(a.c_str(), nullptr, 16);
                       uint32_t rb = strtoul(b.c_str(), nullptr, 16);
                       uint32_t dur = mws.webserver->hasArg("d") ? mws.webserver->arg("d").toInt() : 60000;
                       DisplayManager.hub75Checkerboard(ra, rb, dur);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
    // Debug: triangular brightness sweep 0..255..0. cycleMs = full period.
    // Usage: POST /api/hub75/brisweep?ms=4000   (0 stops and restores app brightness)
    mws.addHandler("/api/hub75/brisweep", HTTP_ANY, []()
                   {
                       uint32_t ms = mws.webserver->hasArg("ms") ? mws.webserver->arg("ms").toInt() : 4000;
                       DisplayManager.hub75BrightnessSweep(ms);
                       mws.webserver->send(200, F("text/plain"), F("OK"));
                   });
#endif
    mws.addHandler("/api/rtttl", HTTP_POST, []()
                   { mws.webserver->send(200,F("text/plain"),F("OK")); PeripheryManager.playRTTTLString(mws.webserver->arg("plain").c_str()); });
    mws.addHandler("/api/sound", HTTP_POST, []()
                   { if (PeripheryManager.parseSound(mws.webserver->arg("plain").c_str())){
                    mws.webserver->send(200,F("text/plain"),F("OK")); 
                   }else{
                    mws.webserver->send(404,F("text/plain"),F("FileNotFound"));  
                   }; });

    mws.addHandler("/api/moodlight", HTTP_POST, []()
                   {
                    if (DisplayManager.moodlight(mws.webserver->arg("plain").c_str()))
                    {
                        mws.webserver->send(200, F(F("text/plain")), F("OK"));
                    }
                    else
                    {
                        mws.webserver->send(500, F("text/plain"), F("ErrorParsingJson"));
                    } });
    mws.addHandler("/api/notify", HTTP_POST, []()
                   {
                       if (DisplayManager.generateNotification(1,mws.webserver->arg("plain").c_str()))
                       {
                        mws.webserver->send(200, F("text/plain"), F("OK"));
                       }else{
                        mws.webserver->send(500, F("text/plain"), F("ErrorParsingJson"));
                       } });
    mws.addHandler("/api/nextapp", HTTP_ANY, []()
                   { DisplayManager.nextApp(); mws.webserver->send(200,F("text/plain"),F("OK")); });
    // /fullscreen (screenfull_html) and /backup (backup_html) removed —
    // convenience pages not used on hub75, freed ~4.5 KB flash.
    mws.addHandler("/screen", HTTP_GET, []()
                   { mws.webserver->send(200, "text/html", screen_html); });
    mws.addHandler("/api/previousapp", HTTP_POST, []()
                   { DisplayManager.previousApp(); mws.webserver->send(200,F("text/plain"),F("OK")); });
    mws.addHandler("/api/notify/dismiss", HTTP_ANY, []()
                   { DisplayManager.dismissNotify(); mws.webserver->send(200,F("text/plain"),F("OK")); });
    mws.addHandler("/api/apps", HTTP_POST, []()
                   { DisplayManager.updateAppVector(mws.webserver->arg("plain").c_str()); mws.webserver->send(200,F("text/plain"),F("OK")); });
    mws.addHandler(
        "/api/switch", HTTP_POST, []()
        {
        if (DisplayManager.switchToApp(mws.webserver->arg("plain").c_str()))
        {
            mws.webserver->send(200, F("text/plain"), F("OK"));
        }
        else
        {
            mws.webserver->send(500, F("text/plain"), F("FAILED"));
        } });
    mws.addHandler("/api/apps", HTTP_GET, []()
                   { mws.webserver->send_P(200, "application/json", DisplayManager.getAppsWithIcon().c_str()); });
    mws.addHandler("/api/settings", HTTP_POST, []()
                   { DisplayManager.setNewSettings(mws.webserver->arg("plain").c_str()); mws.webserver->send(200,F("text/plain"),F("OK")); });
    mws.addHandler("/api/erase", HTTP_ANY, []()
                   { ServerManager.erase();  mws.webserver->send(200,F("text/plain"),F("OK"));delay(200); ESP.restart(); });
    mws.addHandler("/api/resetSettings", HTTP_ANY, []()
                   { formatSettings();   mws.webserver->send(200,F("text/plain"),F("OK"));delay(200); ESP.restart(); });
    mws.addHandler("/api/reorder", HTTP_POST, []()
                   { DisplayManager.reorderApps(mws.webserver->arg("plain").c_str()); mws.webserver->send(200,F("text/plain"),F("OK")); });
    mws.addHandler("/api/settings", HTTP_GET, []()
                   { mws.webserver->send_P(200, "application/json", DisplayManager.getSettings().c_str()); });
    mws.addHandler("/api/custom", HTTP_POST, []()
                   { 
                    if (DisplayManager.parseCustomPage(mws.webserver->arg("name"),mws.webserver->arg("plain").c_str(),false)){
                        mws.webserver->send(200,F("text/plain"),F("OK")); 
                    }else{
                        mws.webserver->send(500,F("text/plain"),F("ErrorParsingJson")); 
                    } });
    mws.addHandler("/api/stats", HTTP_GET, []()
                   { mws.webserver->send_P(200, "application/json", DisplayManager.getStats().c_str()); });
    mws.addHandler("/api/screen", HTTP_GET, []()
                   {
                     CRGB *leds = DisplayManager.getLedsCopy();
                     const int total = DisplayManager.getMatrixWidth() * DisplayManager.getMatrixHeight();
                     // Build in 512-byte chunks to avoid large heap allocation
                     // while still batching writes (2048 individual sendContent
                     // calls block the WiFi stack).
                     char chunk[512];
                     int pos = 0;
                     mws.webserver->setContentLength(CONTENT_LENGTH_UNKNOWN);
                     mws.webserver->send(200, "application/json", "");
                     mws.webserver->sendContent("[", 1);
                     for (int i = 0; i < total; i++)
                     {
                       uint32_t c = ((uint32_t)leds[i].r << 16) |
                                    ((uint32_t)leds[i].g << 8)  |
                                    leds[i].b;
                       pos += snprintf(chunk + pos, sizeof(chunk) - pos,
                                       i ? ",%lu" : "%lu", (unsigned long)c);
                       if (pos > 450 || i == total - 1)
                       {
                         mws.webserver->sendContent(chunk, pos);
                         pos = 0;
                       }
                     }
                     mws.webserver->sendContent("]", 1);
                     mws.webserver->sendContent("", 0);
                   });
    mws.addHandler("/api/indicator1", HTTP_POST, []()
                   { 
                    if (DisplayManager.indicatorParser(1,mws.webserver->arg("plain").c_str())){
                     mws.webserver->send(200,F("text/plain"),F("OK")); 
                    }else{
                         mws.webserver->send(500,F("text/plain"),F("ErrorParsingJson")); 
                    } });
    mws.addHandler("/api/indicator2", HTTP_POST, []()
                   { 
                    if (DisplayManager.indicatorParser(2,mws.webserver->arg("plain").c_str())){
                     mws.webserver->send(200,F("text/plain"),F("OK")); 
                    }else{
                         mws.webserver->send(500,F("text/plain"),F("ErrorParsingJson")); 
                    } });
    mws.addHandler("/api/indicator3", HTTP_POST, []()
                   { 
                    if (DisplayManager.indicatorParser(3,mws.webserver->arg("plain").c_str())){
                     mws.webserver->send(200,F("text/plain"),F("OK")); 
                    }else{
                         mws.webserver->send(500,F("text/plain"),F("ErrorParsingJson")); 
                    } });
    mws.addHandler("/api/doupdate", HTTP_POST, []()
                   { 
                    if (UpdateManager.checkUpdate(true)){
                        mws.webserver->send(200,F("text/plain"),F("OK"));
                        UpdateManager.updateFirmware();
                    }else{
                        mws.webserver->send(404,F("text/plain"),"NoUpdateFound");    
                    } });
    mws.addHandler("/api/r2d2", HTTP_POST, []()
                   { PeripheryManager.r2d2(mws.webserver->arg("plain").c_str()); mws.webserver->send(200,F("text/plain"),F("OK")); });
}

void ServerManager_::setup()
{
    esp_wifi_set_max_tx_power(80); // 82 * 0.25 dBm = 20.5 dBm
    esp_wifi_set_ps(WIFI_PS_NONE); // Power Saving deaktivieren
    if (!local_IP.fromString(NET_IP) || !gateway.fromString(NET_GW) || !subnet.fromString(NET_SN) || !primaryDNS.fromString(NET_PDNS) || !secondaryDNS.fromString(NET_SDNS))
        NET_STATIC = false;
    if (NET_STATIC)
    {
        WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
    }
    WiFi.setHostname(HOSTNAME.c_str()); // define hostname
    myIP = mws.startWiFi(AP_TIMEOUT * 1000, HOSTNAME.c_str(), "12345678");
    isConnected = !(myIP == IPAddress(192, 168, 4, 1));
    if (DEBUG_MODE)
        DEBUG_PRINTF("My IP: %d.%d.%d.%d", myIP[0], myIP[1], myIP[2], myIP[3]);
    mws.setAuth(AUTH_USER, AUTH_PASS);
    if (isConnected)
    {
        mws.addOptionBox("Network");
        mws.addOption("Static IP", NET_STATIC);
        mws.addOption("Local IP", NET_IP);
        mws.addOption("Gateway", NET_GW);
        mws.addOption("Subnet", NET_SN);
        mws.addOption("Primary DNS", NET_PDNS);
        mws.addOption("Secondary DNS", NET_SDNS);
        mws.addOptionBox("MQTT");
        mws.addOption("Broker", MQTT_HOST);
        mws.addOption("Port", MQTT_PORT);
        mws.addOption("Username", MQTT_USER);
        mws.addOption("Password", MQTT_PASS);
        mws.addOption("Prefix", MQTT_PREFIX);
        mws.addOption("Homeassistant Discovery", HA_DISCOVERY);
        mws.addOptionBox("Time");
        mws.addOption("NTP Server", NTP_SERVER);
        mws.addOption("Timezone", NTP_TZ);
        mws.addHTML("<p>Find your timezone at <a href='https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv' target='_blank' rel='noopener noreferrer'>posix_tz_db</a>.</p>", "tz_link");
        mws.addOptionBox("Icons");
        mws.addHTML(custom_html, "icon_html");
        mws.addCSS(custom_css);
        mws.addJavascript(custom_script);
        mws.addOptionBox("Auth");
        mws.addOption("Auth Username", AUTH_USER);
        mws.addOption("Auth Password", AUTH_PASS);
        mws.addHandler("/save", HTTP_POST, saveHandler);
        addHandler();
        udp.begin(localUdpPort);
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Webserver loaded"));
    }
    mws.addHandler("/version", HTTP_GET, versionHandler);
    mws.begin(WEB_PORT);

    if (!MDNS.begin(HOSTNAME))
    {
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Error starting mDNS"));
    }
    else
    {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService("awtrix", "tcp", 80);
        MDNS.addServiceTxt("awtrix", "tcp", "id", uniqueID);
        MDNS.addServiceTxt("awtrix", "tcp", "name", HOSTNAME.c_str());
        MDNS.addServiceTxt("awtrix", "tcp", "type", "awtrix3");
    }

    configTzTime(NTP_TZ.c_str(), NTP_SERVER.c_str());
    tm timeInfo;
    getLocalTime(&timeInfo);
    TCPserver.begin();
    TCPserver.setNoDelay(true);
}

void ServerManager_::tick()
{
    mws.run();

    if (!AP_MODE)
    {
        int packetSize = udp.parsePacket();
        if (packetSize)
        {
            int len = udp.read(incomingPacket, 255);
            if (len > 0)
            {
                incomingPacket[len] = 0;
            }
            if (strcmp(incomingPacket, "FIND_AWTRIX") == 0)
            {
                udp.beginPacket(udp.remoteIP(), 4211);
                if (WEB_PORT != 80)
                {
                    char buffer[128];
                    sprintf(buffer, "%s:%d", HOSTNAME.c_str(), WEB_PORT);
                    udp.printf(buffer);
                }
                else
                {
                    udp.printf(HOSTNAME.c_str());
                }

                udp.endPacket();
            }
        }
    }

    if (!currentClient || !currentClient.connected()) {
        if (TCPserver.hasClient()) {
            if (currentClient) {
                currentClient.stop();
                Serial.println("Vorheriger Client getrennt, um neuen Client zu akzeptieren.");
            }
            currentClient = TCPserver.available();
            Serial.println("Neuer Client verbunden.");
        }
    }

    if (currentClient && currentClient.connected()) {
        while (currentClient.available()) {
            char incomingByte = currentClient.read();            
            if (incomingByte == '\n') {
                dataBuffer[bufferIndex] = '\0';               
                GameManager.ControllerInput(dataBuffer);
                bufferIndex = 0;
            }
            else if (incomingByte != '\r') {
                if (bufferIndex < BUFFER_SIZE - 1) {
                    dataBuffer[bufferIndex++] = incomingByte;
                }
                else {
                    bufferIndex = 0;
                }
            }
        }
    }
}

void ServerManager_::sendTCP(String message)
{
    if (currentClient && currentClient.connected()) {
        currentClient.print(message);
    }
}

void ServerManager_::loadSettings()
{
    if (LittleFS.exists("/DoNotTouch.json"))
    {
        File file = LittleFS.open("/DoNotTouch.json", "r");
        DynamicJsonDocument doc(file.size() * 1.33);
        DeserializationError error = deserializeJson(doc, file);
        if (error)
            return;

        NTP_SERVER = doc["NTP Server"].as<String>();
        NTP_TZ = doc["Timezone"].as<String>();
        MQTT_HOST = doc["Broker"].as<String>();
        MQTT_PORT = doc["Port"].as<uint16_t>();
        MQTT_USER = doc["Username"].as<String>();
        MQTT_PASS = doc["Password"].as<String>();
        MQTT_PREFIX = doc["Prefix"].as<String>();
        MQTT_PREFIX.trim();
        NET_STATIC = doc["Static IP"];
        HA_DISCOVERY = doc["Homeassistant Discovery"];
        NET_IP = doc["Local IP"].as<String>();
        NET_GW = doc["Gateway"].as<String>();
        NET_SN = doc["Subnet"].as<String>();
        NET_PDNS = doc["Primary DNS"].as<String>();
        NET_SDNS = doc["Secondary DNS"].as<String>();
        if (doc["Auth Username"].is<String>())
            AUTH_USER = doc["Auth Username"].as<String>();
        if (doc["Auth Password"].is<String>())
            AUTH_PASS = doc["Auth Password"].as<String>();

        file.close();
        DisplayManager.applyAllSettings();
        if (DEBUG_MODE)
            DEBUG_PRINTLN(F("Webserver configuration loaded"));
        doc.clear();
        return;
    }
    else if (DEBUG_MODE)
        DEBUG_PRINTLN(F("Webserver configuration file not exist"));
    return;
}

void ServerManager_::sendButton(byte btn, bool state)
{
    if (BUTTON_CALLBACK == "")
        return;
    static bool btn0State, btn1State, btn2State;
    String payload;
    switch (btn)
    {
    case 0:
        if (btn0State != state)
        {
            btn0State = state;
            payload = "button=left&state=" + String(state) + "&uid=" + uniqueID;
        }
        break;
    case 1:
        if (btn1State != state)
        {
            btn1State = state;
            payload = "button=middle&state=" + String(state) + "&uid=" + uniqueID;
        }
        break;
    case 2:
        if (btn2State != state)
        {
            btn2State = state;
            payload = "button=right&state=" + String(state) + "&uid=" + uniqueID;
        }
        break;
    default:
        return;
    }
    if (!payload.isEmpty())
    {
        HTTPClient http;
        http.begin(BUTTON_CALLBACK);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        http.POST(payload);
        http.end();
    }
}
