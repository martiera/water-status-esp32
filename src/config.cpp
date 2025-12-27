#include "config.h"
#include <string.h>

ConfigManager::ConfigManager() {
}

void ConfigManager::begin() {
    preferences.begin("water-status", false);
    load();
}

void ConfigManager::setDefaults() {
    // Default WiFi (to be configured via web)
    strcpy(config.wifi_ssid, "");
    strcpy(config.wifi_password, "");
    
    // Default Home Assistant settings
    strcpy(config.ha_url, "http://homeassistant.local:8123");
    strcpy(config.ha_token, "");
    
    // Default Entity IDs (empty - to be configured)
    strcpy(config.entity_tank_temp, "");
    strcpy(config.entity_out_pipe_temp, "");
    strcpy(config.entity_heating_in_temp, "");
    strcpy(config.entity_heating_out_temp, "");
    strcpy(config.entity_room_temp, "");
    
    // Default thresholds (Celsius)
    config.min_tank_temp = 52.0;         // 52°C minimum for bath
    config.min_out_pipe_temp = 38.0;     // 38°C minimum out pipe
    
    // Display settings
    config.screen_brightness = 120;
    config.celsius = true;
    
    // Polling interval
    config.poll_interval = 10;           // 10 seconds
}

void ConfigManager::load() {
    // Check if config exists
    if (!preferences.isKey("wifi_ssid")) {
        Serial.println("No config found, setting defaults");
        setDefaults();
        save();
        return;
    }
    
    // Load WiFi
    preferences.getString("wifi_ssid", config.wifi_ssid, sizeof(config.wifi_ssid));
    preferences.getString("wifi_pass", config.wifi_password, sizeof(config.wifi_password));
    
    // Load Home Assistant settings
    preferences.getString("ha_url", config.ha_url, sizeof(config.ha_url));
    preferences.getString("ha_token", config.ha_token, sizeof(config.ha_token));
    
    // Load Entity IDs
    preferences.getString("ent_tank", config.entity_tank_temp, sizeof(config.entity_tank_temp));
    preferences.getString("ent_out", config.entity_out_pipe_temp, sizeof(config.entity_out_pipe_temp));
    preferences.getString("ent_heat_in", config.entity_heating_in_temp, sizeof(config.entity_heating_in_temp));
    preferences.getString("ent_heat_out", config.entity_heating_out_temp, sizeof(config.entity_heating_out_temp));
    preferences.getString("ent_room", config.entity_room_temp, sizeof(config.entity_room_temp));
    
    Serial.println("Loaded config:");
    Serial.print("  HA URL: ");
    Serial.println(config.ha_url);
    Serial.print("  Token len: ");
    Serial.println(strlen(config.ha_token));
    Serial.print("  Tank entity: '");
    Serial.print(config.entity_tank_temp);
    Serial.println("'");
    Serial.print("  Out entity: '");
    Serial.print(config.entity_out_pipe_temp);
    Serial.println("'");
    
    // Load thresholds
    config.min_tank_temp = preferences.getFloat("min_tank", 52.0);
    config.min_out_pipe_temp = preferences.getFloat("min_out", 38.0);
    
    // Load display settings
    config.screen_brightness = preferences.getInt("brightness", 200);
    config.celsius = preferences.getBool("celsius", true);
    
    // Load polling interval
    config.poll_interval = preferences.getInt("poll_int", 10);
}

void ConfigManager::save() {
    // Save WiFi
    preferences.putString("wifi_ssid", config.wifi_ssid);
    preferences.putString("wifi_pass", config.wifi_password);
    
    // Save Home Assistant settings
    preferences.putString("ha_url", config.ha_url);
    preferences.putString("ha_token", config.ha_token);
    
    // Save Entity IDs
    preferences.putString("ent_tank", config.entity_tank_temp);
    preferences.putString("ent_out", config.entity_out_pipe_temp);
    preferences.putString("ent_heat_in", config.entity_heating_in_temp);
    preferences.putString("ent_heat_out", config.entity_heating_out_temp);
    preferences.putString("ent_room", config.entity_room_temp);
    
    // Save thresholds
    preferences.putFloat("min_tank", config.min_tank_temp);
    preferences.putFloat("min_out", config.min_out_pipe_temp);
    
    // Save display settings
    preferences.putInt("brightness", config.screen_brightness);
    preferences.putBool("celsius", config.celsius);
    
    // Save polling interval
    preferences.putInt("poll_int", config.poll_interval);
}

void ConfigManager::setWiFi(const char* ssid, const char* password) {
    strncpy(config.wifi_ssid, ssid, sizeof(config.wifi_ssid) - 1);
    strncpy(config.wifi_password, password, sizeof(config.wifi_password) - 1);
}

void ConfigManager::setHA(const char* url, const char* token) {
    strncpy(config.ha_url, url, sizeof(config.ha_url) - 1);
    strncpy(config.ha_token, token, sizeof(config.ha_token) - 1);
}

void ConfigManager::setEntities(const char* tank, const char* outPipe, const char* heatIn, const char* heatOut, const char* room) {
    strncpy(config.entity_tank_temp, tank, sizeof(config.entity_tank_temp) - 1);
    strncpy(config.entity_out_pipe_temp, outPipe, sizeof(config.entity_out_pipe_temp) - 1);
    strncpy(config.entity_heating_in_temp, heatIn, sizeof(config.entity_heating_in_temp) - 1);
    strncpy(config.entity_heating_out_temp, heatOut, sizeof(config.entity_heating_out_temp) - 1);
    strncpy(config.entity_room_temp, room, sizeof(config.entity_room_temp) - 1);
}

void ConfigManager::setThresholds(float minTank, float minOutPipe) {
    config.min_tank_temp = minTank;
    config.min_out_pipe_temp = minOutPipe;
}
