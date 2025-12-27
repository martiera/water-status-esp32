#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

// Temperature data structure
struct TemperatureData {
    float tankTemp;
    float outPipeTemp;
    float heatingInTemp;
    float heatingOutTemp;
    float roomTemp;
    float previousTankTemp;
    bool tankValid;
    bool outPipeValid;
    bool heatingInValid;
    bool heatingOutValid;
    bool roomValid;
    bool tankDropping;
    unsigned long lastUpdate;
    unsigned long lastTankUpdate;
    unsigned long lastHotWaterActivity;  // Track hot water system activity
};

class DisplayManager {
private:
    TemperatureData tempData;
    bool bathReady;
    bool previousBathReady;
    float minTankTemp;
    float minOutPipeTemp;
    bool useCelsius;
    bool needsRedraw;
    bool showingBathStatus;  // Track display mode
    bool showingBathImage;   // For alternating bath/room display
    unsigned long lastDisplayToggle;  // Time of last toggle
    
    void drawHeader();
    void drawRoomTemperature();
    void drawTemperatures();
    void drawStatus();
    void drawBathtubIcon(int x, int y, int size, bool ready);
    void drawThermometerBar(int x, int y, int width, int height, float temp, float minTemp, float maxTemp, uint16_t color);
    
    float convertTemp(float temp);
    const char* getTempUnit();
    
public:
    DisplayManager();
    void begin(int brightness = 200);
    void setBrightness(int brightness);
    void setTemperatureUnit(bool celsius);
    void setThresholds(float minTank, float minOutPipe);
    
    void updateTemperature(int sensor, float value);
    void updateBathStatus(bool ready);
    
    void showConfigMode();
    void showIPAddress(IPAddress ip);
    void showStartupScreen(IPAddress ip);
    void refresh();
};

#endif
