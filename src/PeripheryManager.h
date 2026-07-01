#ifndef PeripheryManager_h
#define PeripheryManager_h
#include <Arduino.h>
#include "HardwareConfig.h"
#ifdef HAS_BUTTONS
#include <EasyButton.h>
#endif
#define DFMINI_MP3_BOOT "1"
#define DFMINI_MP3_ALARM "2"
#define DFMINI_MP3_TIMER "2"
#define DFMINI_MP3_CLICK "5"
#define DFMINI_MP3_CLICK_ON "3"
#define DFMINI_MP3_ENTER "4"

class PeripheryManager_
{
private:
    PeripheryManager_();
#ifdef HAS_BATTERY
    const int BatReadings = 10;
    uint16_t TotalBatReadings[10];
#endif
    int readIndex = 0;
    uint16_t total = 0;
    uint16_t average = 0;
    const int LDRReadings = 30;
    uint16_t TotalLDRReadings[30];
    int sampleIndex = 0;
    unsigned long previousMillis = 0;
    const unsigned long interval = 1000;

public:
#ifdef HAS_BUTTONS
    EasyButton *buttonL;
    EasyButton *buttonR;
    EasyButton *buttonS;
    EasyButton *buttonRST;
#endif

    static PeripheryManager_ &getInstance();
    void setup();
    void tick();
    void playBootSound();
    const char *playFromFile(String file);
    const char *playRTTTLString(String rtttl);
    bool parseSound(const char *json);
    bool isPlaying();
    void stopSound();
    void r2d2(const char* msg);
    uint8_t getMatrixPin();
    void setVolume(uint8_t);
    unsigned long long readUptime();
};

extern PeripheryManager_ &PeripheryManager;
#endif
