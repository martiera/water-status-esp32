#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>

/**
 * @brief Temperature sensor data structure
 * 
 * Stores current readings and state for all temperature sensors.
 * Includes validity flags to handle sensor failures gracefully.
 * Tracks hot water activity for intelligent display mode switching.
 */
struct TemperatureData {
    float tankTemp;
    float outPipeTemp;
    float heatingInTemp;
    float roomTemp;
    float previousTankTemp;
    bool tankValid;
    bool outPipeValid;
    bool heatingInValid;
    bool roomValid;
    bool tankDropping;
    unsigned long lastUpdate;
    unsigned long lastTankUpdate;
    unsigned long lastHotWaterActivity;  // Track hot water system activity
    bool heatingActive;  // Track if heating is currently running
};

/**
 * @brief Display manager for TFT LCD interface
 * 
 * Manages all display operations including:
 * - Temperature visualization
 * - Bath readiness status (STOP sign or bath image)
 * - Room temperature display
 * - Heating activity animations
 * - Brightness control
 * 
 * Uses LovyanGFX library for hardware acceleration.
 * Implements smart refresh logic to minimize redraws.
 */
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
    void drawHeatingIndicator();
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
    void updateHeatingStatus(bool active);
    
    void showConfigMode();
    void showIPAddress(IPAddress ip);
    void showStartupScreen(IPAddress ip);
    void refresh();
};

#endif
