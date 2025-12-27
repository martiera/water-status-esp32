#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Preferences.h>

// Configuration structure
struct Config {
    // WiFi settings
    char wifi_ssid[64];
    char wifi_password[64];
    
    // Home Assistant settings
    char ha_url[128];                    // e.g., "http://192.168.1.100:8123"
    char ha_token[256];                  // Long-lived access token
    
    // Home Assistant Entity IDs for temperature sensors
    char entity_tank_temp[128];          // Hot water in storage tank
    char entity_out_pipe_temp[128];      // Hot water from out pipe
    char entity_heating_in_temp[128];    // Heating pipe incoming
    char entity_room_temp[128];          // Room temperature
    
    // Temperature thresholds for bath readiness
    float min_tank_temp;                 // Minimum tank temperature for bath
    float min_out_pipe_temp;             // Minimum out pipe temperature
    
    // Display settings
    int screen_brightness;               // 0-255
    bool celsius;                        // true = Celsius, false = Fahrenheit
    
    // Polling interval (seconds)
    int poll_interval;                   // How often to fetch from HA
};

class ConfigManager {
private:
    Preferences preferences;
    Config config;
    
public:
    ConfigManager();
    void begin();
    void load();
    void save();
    void setDefaults();
    
    Config& getConfig() { return config; }
    
    void setWiFi(const char* ssid, const char* password);
    void setHA(const char* url, const char* token);
    void setEntities(const char* tank, const char* outPipe, const char* heatIn, const char* room);
    void setThresholds(float minTank, float minOutPipe);
    void setBrightness(int brightness);
};

#endif
