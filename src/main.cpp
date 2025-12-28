#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoOTA.h>
#include "config.h"
#include "display.h"
#include "web_interface.h"

// RGB LED on Waveshare ESP32-C6 1.47" LCD
#define RGB_LED_PIN 8
#define NUM_LEDS 1
Adafruit_NeoPixel rgbLed(NUM_LEDS, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);

// Timing constants
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;
const unsigned long HEATING_CHECK_INTERVAL = 60000;  // Check heating every minute
const unsigned long LED_FLASH_INTERVAL_NOT_READY = 500;
const unsigned long LED_PULSE_INTERVAL_HEATING = 1000;
const unsigned long LED_UPDATE_INTERVAL_READY = 2000;
const unsigned long TEST_STATE_CHANGE_INTERVAL = 3000;
const unsigned long WIFI_RECONNECT_INTERVAL = 30000;
const unsigned long HTTP_TIMEOUT = 5000;

// Heating detection constants
const float HEATING_TEMP_THRESHOLD = 0.5;  // Minimum °C increase to detect heating
const float HEATING_TEMP_DECREASE = 1.0;   // °C decrease to detect heating stopped

// Global objects
ConfigManager configManager;
DisplayManager display;
WebServer server(80);
DNSServer dnsServer;
HTTPClient http;

// AP mode settings
const char* AP_SSID = "Water-Status-AP";
const byte DNS_PORT = 53;
bool apMode = false;

// Status variables
bool wifiConnected = false;
bool haConnected = false;
unsigned long lastHAPoll = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastWiFiCheck = 0;

// Sensor temperatures
float tankTemp = 0.0;
float outPipeTemp = 0.0;
float heatingInTemp = 0.0;
float roomTemp = 0.0;
float previousHeatingInTemp = 0.0;
unsigned long lastHeatingCheck = 0;

// LED state for STOP flashing
bool bathIsReady = false;
bool heatingActive = false;
unsigned long lastLedFlash = 0;
bool ledOn = false;

// Test mode for display verification
bool testMode = false;
int testState = 0;
unsigned long lastTestStateChange = 0;

// Function declarations
void setupWiFi();
void setupOTA();
void pollHomeAssistant();
float fetchHAEntityState(const char* entityId);
void startAPMode();
void startWebServer();
void handleRoot();
void handleConfig();
void handleSaveConfig();
void handleStatus();
void handleScan();
void handleConnect();
void handleNotFound();
void handleHAEntities();
void handleHATest();
void handleDisplayTest();

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for USB CDC to initialize on ESP32-C6
    
    Serial.println();
    Serial.println("========================================");
    Serial.println("=== Water Status ESP32-C6 Starting ===");
    Serial.println("========================================");
    Serial.flush();
    
    // Initialize RGB LED
    rgbLed.begin();
    rgbLed.setBrightness(255);
    rgbLed.clear();
    rgbLed.show();
    
    // Initialize configuration manager
    configManager.begin();
    
    // Load configuration
    Config config = configManager.getConfig();
    
    // Initialize display
    display.begin(config.screen_brightness);
    display.setTemperatureUnit(config.celsius);
    display.setThresholds(config.min_tank_temp, config.min_out_pipe_temp);
    
    Serial.println("Connecting to WiFi...");
    setupWiFi();
    
    if (wifiConnected) {
        // Show startup splash screen with IP address
        IPAddress localIP = WiFi.localIP();
        display.showStartupScreen(localIP);
        delay(3000);  // Show for 3 seconds
        
        // Setup OTA (Over-The-Air) updates
        setupOTA();
        
        Serial.println("Starting web server...");
        startWebServer();
        
        // Initial poll of Home Assistant
        pollHomeAssistant();
    } else {
        Serial.println("Starting AP mode...");
        startAPMode();
        display.showConfigMode();
    }
}

void loop() {
    // Handle AP mode
    if (apMode) {
        dnsServer.processNextRequest();
        server.handleClient();
        return;
    }
    
    // Handle web server and OTA
    if (wifiConnected) {
        server.handleClient();
        ArduinoOTA.handle();
    }
    
    // Poll Home Assistant periodically
    if (wifiConnected && !testMode) {
        Config config = configManager.getConfig();
        unsigned long pollInterval = config.poll_interval * 1000UL;
        unsigned long now = millis();
        
        if (now - lastHAPoll > pollInterval) {
            lastHAPoll = now;
            pollHomeAssistant();
        }
    }
    
    // Test mode - cycle through display states
    if (testMode) {
        unsigned long now = millis();
        if (now - lastTestStateChange > TEST_STATE_CHANGE_INTERVAL) {
            lastTestStateChange = now;
            testState = (testState + 1) % 4;
            
            // Set test conditions based on state
            switch (testState) {
                case 0:  // STOP sign
                    bathIsReady = false;
                    heatingActive = false;
                    display.updateBathStatus(false);
                    display.updateHeatingStatus(false);
                    Serial.println("Test: STOP sign");
                    break;
                case 1:  // Bath ready - bath image (no heating)
                    bathIsReady = true;
                    heatingActive = false;
                    tankTemp = 55.0;
                    outPipeTemp = 42.0;
                    roomTemp = 22.5;
                    display.updateBathStatus(true);
                    display.updateHeatingStatus(false);
                    Serial.println("Test: Bath ready (no heating)");
                    break;
                case 2:  // Bath ready with heating active
                    bathIsReady = true;
                    heatingActive = true;
                    display.updateBathStatus(true);
                    display.updateHeatingStatus(true);
                    Serial.println("Test: Bath ready + heating active");
                    break;
                case 3:  // Room temperature display
                    bathIsReady = true;
                    heatingActive = true;
                    roomTemp = 23.8;
                    display.updateTemperature(3, roomTemp);
                    Serial.println("Test: Room temperature");
                    break;
            }
        }
    }
    
    // Get current time for all timing checks
    unsigned long now = millis();
    
    // WiFi reconnection logic
    if (wifiConnected && WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected! Attempting reconnection...");
        wifiConnected = false;
        lastWiFiCheck = now;
    }
    
    if (!wifiConnected && !apMode && (now - lastWiFiCheck > WIFI_RECONNECT_INTERVAL)) {
        lastWiFiCheck = now;
        Serial.println("Attempting WiFi reconnection...");
        setupWiFi();
    }
    
    // Update display periodically
    if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = now;
        display.refresh();
    }
    
    // LED feedback based on state
    if (!bathIsReady) {
        // Flash LED red when STOP (not ready)
        if (now - lastLedFlash > LED_FLASH_INTERVAL_NOT_READY) {
            lastLedFlash = now;
            ledOn = !ledOn;
            if (ledOn) {
                rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0));  // Red
            } else {
                rgbLed.clear();
            }
            rgbLed.show();
        }
    } else if (heatingActive) {
        // Bath ready AND heating active - pulse orange
        if (now - lastLedFlash > LED_PULSE_INTERVAL_HEATING) {
            lastLedFlash = now;
            ledOn = !ledOn;
            if (ledOn) {
                rgbLed.setPixelColor(0, rgbLed.Color(255, 140, 0));  // Orange
            } else {
                rgbLed.setPixelColor(0, rgbLed.Color(64, 35, 0));  // Dim orange
            }
            rgbLed.show();
        }
    } else {
        // Bath ready, heating inactive - solid green
        if (!ledOn || now - lastLedFlash > LED_UPDATE_INTERVAL_READY) {
            rgbLed.setPixelColor(0, rgbLed.Color(0, 255, 0));  // Green
            rgbLed.show();
            ledOn = true;
            lastLedFlash = now;
        }
    }
    
    delay(100);
}

/**
 * @brief Setup OTA (Over-The-Air) firmware updates
 * 
 * Configures Arduino OTA with:
 * - Hostname: water-status-XXXXXX (last 6 chars of MAC)
 * - Password: water-status
 * - Port: 3232 (default)
 * 
 * Displays update progress on screen and LED.
 */
void setupOTA() {
    // Set OTA hostname to water-status-XXXXXX (MAC address)
    String hostname = "water-status-" + WiFi.macAddress().substring(12);
    hostname.replace(":", "");
    ArduinoOTA.setHostname(hostname.c_str());
    
    // Set OTA password for security
    ArduinoOTA.setPassword("water-status");
    
    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
        Serial.println("OTA Update Start: " + type);
        
        // Set LED to blue during update
        rgbLed.setPixelColor(0, rgbLed.Color(0, 0, 255));
        rgbLed.show();
    });
    
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA Update Complete");
        
        rgbLed.setPixelColor(0, rgbLed.Color(0, 255, 0));
        rgbLed.show();
    });
    
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        unsigned int percent = (progress / (total / 100));
        Serial.printf("OTA Progress: %u%%\r", percent);
    });
    
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA Error[%u]: ", error);
        String errorMsg = "";
        if (error == OTA_AUTH_ERROR) errorMsg = "Auth Failed";
        else if (error == OTA_BEGIN_ERROR) errorMsg = "Begin Failed";
        else if (error == OTA_CONNECT_ERROR) errorMsg = "Connect Failed";
        else if (error == OTA_RECEIVE_ERROR) errorMsg = "Receive Failed";
        else if (error == OTA_END_ERROR) errorMsg = "End Failed";
        
        Serial.println(errorMsg);
        
        rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0));
        rgbLed.show();
    });
    
    ArduinoOTA.begin();
    Serial.println("OTA Ready");
    Serial.print("Hostname: ");
    Serial.println(hostname);
}

/**
 * @brief Connect to configured WiFi network
 * 
 * Attempts to connect to the WiFi network specified in configuration.
 * Will try up to 20 times (10 seconds) before giving up.
 * Updates wifiConnected flag and displays IP address on success.
 */
void setupWiFi() {
    Config config = configManager.getConfig();
    
    if (strlen(config.wifi_ssid) == 0) {
        Serial.println("No WiFi configured!");
        return;
    }
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid, config.wifi_password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\nWiFi connected!");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
        
        // Show IP address on display for 3 seconds
        display.showIPAddress(WiFi.localIP());
        delay(3000);
    } else {
        Serial.println("\nWiFi connection failed!");
    }
}

/**
 * @brief Fetch temperature from a single Home Assistant entity
 * 
 * @param entityId The entity ID to fetch (e.g., "sensor.tank_temperature")
 * @return float The temperature value, or 0.0 if fetch failed or entity unavailable
 * 
 * Uses string-based parsing to extract the "state" field from JSON response.
 * This avoids ArduinoJson memory allocation issues with large responses.
 * Enables HTTP connection reuse for better performance.
 */
float fetchHAEntityState(const char* entityId) {
    if (strlen(entityId) == 0) {
        return 0.0;
    }
    
    Config config = configManager.getConfig();
    
    if (strlen(config.ha_url) == 0 || strlen(config.ha_token) == 0) {
        return 0.0;
    }
    
    String url = String(config.ha_url) + "/api/states/" + entityId;
    
    Serial.print("    Fetching: ");
    Serial.println(url);
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + config.ha_token);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT);
    http.setReuse(true);  // Enable connection reuse for better performance
    
    int httpCode = http.GET();
    float temperature = 0.0;
    
    Serial.print("    HTTP code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("    Response length: ");
        Serial.println(payload.length());
        
        // Simple string extraction for "state" field - more reliable than JSON parsing
        // Format: {"entity_id":"...","state":"54.25","attributes":...}
        int stateStart = payload.indexOf("\"state\":\"");
        if (stateStart > 0) {
            stateStart += 9;  // Skip past "state":"
            int stateEnd = payload.indexOf("\"", stateStart);
            if (stateEnd > stateStart) {
                String stateStr = payload.substring(stateStart, stateEnd);
                Serial.print("    State string: '");
                Serial.print(stateStr);
                Serial.println("'");
                
                if (stateStr != "unavailable" && stateStr != "unknown") {
                    temperature = stateStr.toFloat();
                    Serial.print("    Parsed temp: ");
                    Serial.println(temperature);
                }
            }
        }
    } else {
        Serial.print("HA fetch error for ");
        Serial.print(entityId);
        Serial.print(": ");
        Serial.println(httpCode);
    }
    
    http.end();
    return temperature;
}

/**
 * @brief Poll all configured temperature sensors from Home Assistant
 * 
 * Fetches current state of all configured temperature entities via HA REST API.
 * Uses string parsing instead of JSON to avoid memory allocation issues.
 * Updates display and checks bath readiness after fetching all sensors.
 * 
 * Also performs heating activity detection by comparing temperature changes
 * over 60 second intervals.
 */
void pollHomeAssistant() {
    Config config = configManager.getConfig();
    
    Serial.println("Polling Home Assistant...");
    Serial.print("  HA URL: ");
    Serial.println(config.ha_url);
    Serial.print("  Token length: ");
    Serial.println(strlen(config.ha_token));
    
    bool anySuccess = false;
    
    // Fetch tank temperature
    Serial.print("  Tank entity: '");
    Serial.print(config.entity_tank_temp);
    Serial.println("'");
    if (strlen(config.entity_tank_temp) > 0) {
        float temp = fetchHAEntityState(config.entity_tank_temp);
        if (temp != 0.0 || tankTemp == 0.0) {
            tankTemp = temp;
            display.updateTemperature(0, tankTemp);
            anySuccess = true;
        }
    }
    
    // Fetch out pipe temperature
    Serial.print("  Out pipe entity: '");
    Serial.print(config.entity_out_pipe_temp);
    Serial.println("'");
    if (strlen(config.entity_out_pipe_temp) > 0) {
        float temp = fetchHAEntityState(config.entity_out_pipe_temp);
        if (temp != 0.0 || outPipeTemp == 0.0) {
            outPipeTemp = temp;
            display.updateTemperature(1, outPipeTemp);
            anySuccess = true;
        }
    }
    
    // Fetch heating in temperature
    if (strlen(config.entity_heating_in_temp) > 0) {
        float temp = fetchHAEntityState(config.entity_heating_in_temp);
        if (temp != 0.0 || heatingInTemp == 0.0) {
            heatingInTemp = temp;
            display.updateTemperature(2, heatingInTemp);
            anySuccess = true;
        }
    }
    
    // Fetch room temperature
    if (strlen(config.entity_room_temp) > 0) {
        float temp = fetchHAEntityState(config.entity_room_temp);
        if (temp != 0.0 || roomTemp == 0.0) {
            roomTemp = temp;
            display.updateTemperature(3, roomTemp);
            anySuccess = true;
        }
    }
    
    haConnected = anySuccess;
    
    Serial.print("Any success: ");
    Serial.println(anySuccess ? "YES" : "NO");
    
    // Detect heating activity (heating in temp increased significantly)
    unsigned long now = millis();
    if (now - lastHeatingCheck > HEATING_CHECK_INTERVAL) {
        // Only compare if we have valid readings (not initial 0.0) and enough time has passed
        if (previousHeatingInTemp > 0.0 && heatingInTemp > 0.0 && lastHeatingCheck > 0) {
            float tempDiff = heatingInTemp - previousHeatingInTemp;
            
            // Calculate rate of change per minute
            float minutesElapsed = (now - lastHeatingCheck) / 60000.0;
            float ratePerMinute = tempDiff / minutesElapsed;
            
            if (ratePerMinute > HEATING_TEMP_THRESHOLD) {
                heatingActive = true;
                Serial.print("Heating ACTIVE detected: +");
                Serial.print(tempDiff, 2);
                Serial.print("°C in ");
                Serial.print(minutesElapsed, 1);
                Serial.println(" min");
            } else if (ratePerMinute < -HEATING_TEMP_DECREASE) {
                heatingActive = false;
                Serial.println("Heating INACTIVE (temp dropping)");
            }
        }
        
        // Update previous reading only if current reading is valid
        if (heatingInTemp > 0.0) {
            previousHeatingInTemp = heatingInTemp;
        }
        lastHeatingCheck = now;
    }
    
    // Check bath readiness with flexible logic:
    // If out pipe meets threshold → ready
    // OR if out pipe is colder than tank, check tank threshold → ready
    bathIsReady = (outPipeTemp >= config.min_out_pipe_temp) || 
                  (outPipeTemp < tankTemp && tankTemp >= config.min_tank_temp);
    display.updateBathStatus(bathIsReady);
    display.updateHeatingStatus(heatingActive);
    Serial.print("Bath ready: ");
    Serial.println(bathIsReady ? "YES" : "NO");
    Serial.print("  Tank: "); Serial.print(tankTemp); Serial.print(" >= "); Serial.println(config.min_tank_temp);
    Serial.print("  OutPipe: "); Serial.print(outPipeTemp); Serial.print(" >= "); Serial.println(config.min_out_pipe_temp);
    Serial.print("  OutPipe < Tank: "); Serial.println(outPipeTemp < tankTemp ? "YES" : "NO");
    Serial.print("  Heating active: "); Serial.println(heatingActive ? "YES" : "NO");
}

// Fetch list of temperature sensors from Home Assistant
void handleHAEntities() {
    Config config = configManager.getConfig();
    
    // Get from POST body for security
    String ha_url = server.arg("ha_url");
    String ha_token = server.arg("ha_token");
    
    // Use provided params or fall back to saved config
    if (ha_url.length() == 0) ha_url = config.ha_url;
    if (ha_token.length() == 0) ha_token = config.ha_token;
    
    if (ha_url.length() == 0 || ha_token.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"HA not configured\"}");
        return;
    }
    
    // Validate URL format
    if (!ha_url.startsWith("http://") && !ha_url.startsWith("https://")) {
        server.send(400, "application/json", "{\"error\":\"Invalid URL format\"}");
        return;
    }
    
    // Use HA Template API to filter temperature sensors server-side
    String url = ha_url + "/api/template";
    Serial.print("Fetching temperature sensors via template from: ");
    Serial.println(url);
    
    // Jinja2 template that filters only temperature sensors and returns minimal JSON
    String templateBody = "{\"template\":\"[{% set ns = namespace(first=true) %}{% for state in states.sensor | selectattr('attributes.device_class', 'defined') | selectattr('attributes.device_class', 'eq', 'temperature') %}{% if not ns.first %},{% endif %}{% set ns.first = false %}{\\\"id\\\":\\\"{{ state.entity_id }}\\\",\\\"name\\\":\\\"{{ state.name | replace('\\\"', '') }}\\\",\\\"state\\\":\\\"{{ state.state }}\\\",\\\"unit\\\":\\\"{{ state.attributes.unit_of_measurement | default('') }}\\\"}{% endfor %}]\"}";
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + ha_token);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(HTTP_TIMEOUT * 3);  // Longer timeout for template API
    http.setReuse(true);
    
    int httpCode = http.POST(templateBody);
    Serial.print("HA template response: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("Response length: ");
        Serial.println(payload.length());
        
        // The template returns the array directly, wrap it
        String result = "{\"entities\":" + payload + "}";
        server.send(200, "application/json", result);
    } else if (httpCode == 400 || httpCode == 500) {
        // Template API might not work, fall back to simpler approach
        Serial.println("Template API failed, trying simple fetch...");
        http.end();
        
        // Try fetching just a few known entity patterns
        url = ha_url + "/api/states";
        http.begin(url);
        http.addHeader("Authorization", String("Bearer ") + ha_token);
        http.setTimeout(HTTP_TIMEOUT * 3);
        http.setReuse(true);
        
        httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
            // Stream and manually extract temperature sensors without full JSON parsing
            String result = "{\"entities\":[";
            WiFiClient* stream = http.getStreamPtr();
            
            String line;
            bool first = true;
            int count = 0;
            String currentEntity = "";
            bool inEntity = false;
            int braceCount = 0;
            
            // Read and parse minimally - look for entity objects with temperature
            while (stream->available() && count < 50) {
                char c = stream->read();
                
                if (c == '{') {
                    if (braceCount == 1) {  // Start of an entity object
                        inEntity = true;
                        currentEntity = "{";
                    } else if (inEntity) {
                        currentEntity += c;
                    }
                    braceCount++;
                } else if (c == '}') {
                    braceCount--;
                    if (inEntity) {
                        currentEntity += c;
                        if (braceCount == 1) {  // End of entity object
                            inEntity = false;
                            // Check if this is a temperature sensor
                            if (currentEntity.indexOf("\"device_class\":\"temperature\"") > 0 ||
                                currentEntity.indexOf("\"device_class\": \"temperature\"") > 0) {
                                
                                // Extract entity_id
                                int idStart = currentEntity.indexOf("\"entity_id\":\"");
                                if (idStart < 0) idStart = currentEntity.indexOf("\"entity_id\": \"");
                                if (idStart > 0) {
                                    idStart = currentEntity.indexOf("\"", idStart + 12) + 1;
                                    int idEnd = currentEntity.indexOf("\"", idStart);
                                    String entityId = currentEntity.substring(idStart, idEnd);
                                    
                                    // Extract state
                                    int stateStart = currentEntity.indexOf("\"state\":\"");
                                    if (stateStart < 0) stateStart = currentEntity.indexOf("\"state\": \"");
                                    String state = "0";
                                    if (stateStart > 0) {
                                        stateStart = currentEntity.indexOf("\"", stateStart + 8) + 1;
                                        int stateEnd = currentEntity.indexOf("\"", stateStart);
                                        state = currentEntity.substring(stateStart, stateEnd);
                                    }
                                    
                                    // Extract friendly_name
                                    int nameStart = currentEntity.indexOf("\"friendly_name\":\"");
                                    if (nameStart < 0) nameStart = currentEntity.indexOf("\"friendly_name\": \"");
                                    String name = entityId;
                                    if (nameStart > 0) {
                                        nameStart = currentEntity.indexOf("\"", nameStart + 15) + 1;
                                        int nameEnd = currentEntity.indexOf("\"", nameStart);
                                        name = currentEntity.substring(nameStart, nameEnd);
                                    }
                                    
                                    if (state != "unavailable" && state != "unknown") {
                                        if (!first) result += ",";
                                        first = false;
                                        count++;
                                        
                                        result += "{\"id\":\"" + entityId + "\",\"name\":\"" + name + "\",\"state\":\"" + state + "\",\"unit\":\"°C\"}";
                                    }
                                }
                            }
                            currentEntity = "";
                        }
                    }
                } else if (inEntity) {
                    currentEntity += c;
                    // Prevent memory overflow
                    if (currentEntity.length() > 4096) {
                        currentEntity = "";
                        inEntity = false;
                    }
                }
            }
            
            result += "]}";
            Serial.print("Found ");
            Serial.print(count);
            Serial.println(" temperature sensors");
            server.send(200, "application/json", result);
        } else {
            server.send(500, "application/json", "{\"error\":\"HTTP " + String(httpCode) + "\"}");
        }
    } else {
        String error = "{\"error\":\"HTTP error " + String(httpCode) + "\"}";
        server.send(httpCode > 0 ? httpCode : 500, "application/json", error);
    }
    
    http.end();
}

// Test HA connection
void handleHATest() {
    Config config = configManager.getConfig();
    
    // Get credentials from POST body, not URL params (security)
    String ha_url = server.arg("ha_url");
    String ha_token = server.arg("ha_token");
    
    if (ha_url.length() == 0) ha_url = config.ha_url;
    if (ha_token.length() == 0) ha_token = config.ha_token;
    
    Serial.print("Testing HA connection to: ");
    Serial.println(ha_url);
    
    if (ha_url.length() == 0 || ha_token.length() == 0) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing URL or token\"}");
        return;
    }
    
    // Validate URL format
    if (!ha_url.startsWith("http://") && !ha_url.startsWith("https://")) {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid URL format\"}");
        return;
    }
    
    // Use /api/ endpoint which returns API info
    String url = ha_url + "/api/";
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + ha_token);
    http.setTimeout(HTTP_TIMEOUT);
    http.setReuse(true);
    
    int httpCode = http.GET();
    Serial.print("HA test response code: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.print("HA response: ");
        Serial.println(payload);
        server.send(200, "application/json", "{\"success\":true,\"message\":\"Connected to Home Assistant\"}");
    } else if (httpCode == 401) {
        server.send(200, "application/json", "{\"success\":false,\"error\":\"Invalid token\"}");
    } else if (httpCode < 0) {
        String error = "{\"success\":false,\"error\":\"Connection failed: " + http.errorToString(httpCode) + "\"}";
        server.send(200, "application/json", error);
    } else {
        String error = "{\"success\":false,\"error\":\"HTTP " + String(httpCode) + "\"}";
        server.send(200, "application/json", error);
    }
    
    http.end();
}

/**
 * @brief Start Access Point mode for initial WiFi configuration
 * 
 * Creates a captive portal at "Water-Status-AP" SSID.
 * Serves a web interface for WiFi network selection and connection.
 * All DNS queries redirected to the device for captive portal functionality.
 */
void startAPMode() {
    apMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    
    Serial.println("AP Mode started");
    Serial.print("AP SSID: ");
    Serial.println(AP_SSID);
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
    
    // Start DNS server for captive portal
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    // Setup web server routes
    server.on("/", handleRoot);
    server.on("/scan", handleScan);
    server.on("/connect", HTTP_POST, handleConnect);
    server.onNotFound(handleNotFound);
    
    server.begin();
    Serial.println("Web server started");
}

void handleRoot() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>WiFi Setup</title>";
    html += "<style>";
    html += "body{font-family:Arial;margin:0;padding:20px;background:#667eea;}";
    html += ".container{max-width:500px;margin:0 auto;background:#fff;padding:20px;border-radius:10px;}";
    html += "h1{color:#333;text-align:center;}";
    html += "button{background:#667eea;color:#fff;border:none;padding:10px 20px;border-radius:5px;cursor:pointer;width:100%;margin:10px 0;}";
    html += "button:hover{background:#5568d3;}";
    html += "input{width:100%;padding:10px;margin:5px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}";
    html += ".network{padding:10px;margin:5px 0;background:#f0f0f0;border-radius:5px;cursor:pointer;}";
    html += ".network:hover{background:#e0e0e0;}";
    html += "</style></head><body><div class='container'>";
    html += "<h1>WiFi Setup</h1>";
    html += "<button onclick='scanNetworks()'>Scan Networks</button>";
    html += "<div id='networks'></div>";
    html += "<form method='POST' action='/connect'>";
    html += "<input type='text' name='ssid' id='ssid' placeholder='SSID' required>";
    html += "<input type='password' name='password' placeholder='Password'>";
    html += "<button type='submit'>Connect</button>";
    html += "</form></div>";
    html += "<script>";
    html += "function scanNetworks(){";
    html += "fetch('/scan').then(r=>r.json()).then(data=>{";
    html += "let html='';";
    html += "data.networks.forEach(n=>{";
    html += "html+=`<div class='network' onclick='selectNetwork(\"${n.ssid}\")'>${n.ssid} (${n.rssi} dBm)</div>`;";
    html += "});";
    html += "document.getElementById('networks').innerHTML=html;";
    html += "});}";
    html += "function selectNetwork(ssid){document.getElementById('ssid').value=ssid;}";
    html += "</script></body></html>";
    
    server.send(200, "text/html", html);
}

void handleScan() {
    Serial.println("Scanning WiFi networks...");
    int n = WiFi.scanNetworks();
    
    String json = "{\"networks\":[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{";
        json += "\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i));
        json += "}";
    }
    json += "]}";
    
    server.send(200, "application/json", json);
}

void handleConnect() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    
    Serial.println("Attempting to connect to: " + ssid);
    
    // Save configuration
    Config config = configManager.getConfig();
    ssid.toCharArray(config.wifi_ssid, sizeof(config.wifi_ssid));
    password.toCharArray(config.wifi_password, sizeof(config.wifi_password));
    configManager.setWiFi(config.wifi_ssid, config.wifi_password);
    configManager.save();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#667eea;color:#fff;}</style>";
    html += "</head><body>";
    html += "<h1>Saved!</h1>";
    html += "<p>Connecting to " + ssid + "...</p>";
    html += "<p>Device will restart in 3 seconds</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
    
    delay(3000);
    ESP.restart();
}

void handleNotFound() {
    // Redirect all unknown requests to root (captive portal)
    server.sendHeader("Location", "/", true);
    server.send(302, "text/plain", "");
}

/**
 * @brief Initialize web server with all routes
 * 
 * Sets up HTTP endpoints for:
 * - Configuration UI (GET /)
 * - Save settings (POST /save)
 * - Status API (GET /status)
 * - HA integration (POST /ha/test, POST /ha/entities)
 * - Display test mode (GET /display-test)
 * 
 * All sensitive operations use POST to avoid token exposure in logs.
 */
void startWebServer() {
    // Setup web server routes for normal WiFi mode
    server.on("/", handleConfig);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.on("/status", handleStatus);
    server.on("/ha/entities", HTTP_POST, handleHAEntities);  // POST for security
    server.on("/ha/test", HTTP_POST, handleHATest);          // POST for security
    server.on("/display-test", handleDisplayTest);
    server.begin();
    Serial.println("Web server started on port 80");
    Serial.print("Access at: http://");
    Serial.println(WiFi.localIP());
}

void handleConfig() {
    Config config = configManager.getConfig();
    
    String html = "<!DOCTYPE html><html lang='en'><head>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Water Status Configuration</title>";
    html += "<style>";
    html += "body{font-family:Arial;background:#667eea;padding:20px;margin:0;}";
    html += ".container{max-width:600px;margin:0 auto;background:#fff;border-radius:10px;padding:30px;box-shadow:0 10px 40px rgba(0,0,0,0.2);}";
    html += "h1{color:#333;text-align:center;margin-bottom:10px;}";
    html += ".subtitle{text-align:center;color:#666;margin-bottom:30px;}";
    html += ".section{margin-bottom:30px;padding:20px;background:#f8f9fa;border-radius:8px;}";
    html += ".section h2{color:#667eea;margin-bottom:15px;font-size:18px;}";
    html += ".form-group{margin-bottom:15px;}";
    html += "label{display:block;margin-bottom:5px;color:#555;font-weight:500;}";
    html += "input[type='text'],input[type='number'],select{width:100%;padding:10px;border:2px solid #ddd;border-radius:5px;font-size:14px;box-sizing:border-box;}";
    html += "input:focus,select:focus{outline:none;border-color:#667eea;}";
    html += ".btn{width:100%;padding:12px;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:#fff;border:none;border-radius:5px;font-size:16px;font-weight:bold;cursor:pointer;margin-top:10px;}";
    html += ".btn:hover{transform:translateY(-2px);}";
    html += ".btn-secondary{background:#6c757d;margin-top:5px;}";
    html += ".temp-display{padding:15px;background:#e3f2fd;border-radius:5px;margin-bottom:15px;}";
    html += ".temp-value{font-size:24px;font-weight:bold;color:#1976d2;}";
    html += ".status{padding:10px;border-radius:5px;margin-bottom:15px;text-align:center;}";
    html += ".status.success{background:#d4edda;color:#155724;}";
    html += ".status.error{background:#f8d7da;color:#721c24;}";
    html += ".status.loading{background:#fff3cd;color:#856404;}";
    html += "input[type='number']{-moz-appearance:textfield;}";
    html += "input[type='number']::-webkit-inner-spin-button,input[type='number']::-webkit-outer-spin-button{-webkit-appearance:none;margin:0;}";
    html += "</style></head><body>";
    html += "<div class='container'>";
    html += "<h1>🚿 Water Status Monitor</h1>";
    html += "<p class='subtitle'>ESP32-C6 Configuration</p>";
    
    html += "<div class='section'>";
    html += "<h2>📊 Current Temperatures <span id='ha-conn' style='font-size:12px;'></span></h2>";
    html += "<div class='temp-display'>Room: <span class='temp-value' id='t-room'>" + String(roomTemp, 1) + "°C</span></div>";
    html += "<div class='temp-display'>Tank: <span class='temp-value' id='t-tank'>" + String(tankTemp, 1) + "°C</span></div>";
    html += "<div class='temp-display'>Out Pipe: <span class='temp-value' id='t-out'>" + String(outPipeTemp, 1) + "°C</span></div>";
    html += "<div class='temp-display'>Heating In: <span class='temp-value' id='t-hin'>" + String(heatingInTemp, 1) + "°C</span></div>";
    html += "</div>";
    
    html += "<form method='POST' action='/save'>";
    
    html += "<div class='section'>";
    html += "<h2>🏠 Home Assistant</h2>";
    html += "<div id='ha-status'></div>";
    html += "<div class='form-group'><label>HA URL:</label>";
    html += "<input type='text' name='ha_url' id='ha_url' value='" + String(config.ha_url) + "' placeholder='http://homeassistant.local:8123'></div>";
    html += "<div class='form-group'><label>Long-Lived Access Token:</label>";
    html += "<input type='text' name='ha_token' id='ha_token' value='" + String(config.ha_token) + "' placeholder='Your HA token'></div>";
    html += "<button type='button' class='btn btn-secondary' onclick='testHA()'>🔌 Test Connection</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='loadEntities()'>📥 Load Sensors</button>";
    html += "<button type='button' class='btn btn-secondary' onclick='testDisplay()'>🎨 Test Display</button>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h2>📡 Temperature Sensors</h2>";
    html += "<p style='color:#666;font-size:12px;'>Click 'Load Sensors' above to populate dropdowns from Home Assistant</p>";
    
    // Tank temp
    html += "<div class='form-group'><label>Tank Temperature:</label>";
    html += "<select name='entity_tank' id='entity_tank'><option value=''>-- Select sensor --</option>";
    if (strlen(config.entity_tank_temp) > 0) {
        html += "<option value='" + String(config.entity_tank_temp) + "' selected>" + String(config.entity_tank_temp) + "</option>";
    }
    html += "</select></div>";
    
    // Out pipe temp
    html += "<div class='form-group'><label>Out Pipe Temperature:</label>";
    html += "<select name='entity_out' id='entity_out'><option value=''>-- Select sensor --</option>";
    if (strlen(config.entity_out_pipe_temp) > 0) {
        html += "<option value='" + String(config.entity_out_pipe_temp) + "' selected>" + String(config.entity_out_pipe_temp) + "</option>";
    }
    html += "</select></div>";
    
    // Heating in temp
    html += "<div class='form-group'><label>Heating In Temperature:</label>";
    html += "<select name='entity_heat_in' id='entity_heat_in'><option value=''>-- Select sensor --</option>";
    if (strlen(config.entity_heating_in_temp) > 0) {
        html += "<option value='" + String(config.entity_heating_in_temp) + "' selected>" + String(config.entity_heating_in_temp) + "</option>";
    }
    html += "</select></div>";
    
    // Room temp
    html += "<div class='form-group'><label>Room Temperature:</label>";
    html += "<select name='entity_room' id='entity_room'><option value=''>-- Select sensor --</option>";
    if (strlen(config.entity_room_temp) > 0) {
        html += "<option value='" + String(config.entity_room_temp) + "' selected>" + String(config.entity_room_temp) + "</option>";
    }
    html += "</select></div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h2>🌡️ Temperature Thresholds</h2>";
    html += "<div class='form-group'><label>Min Tank Temp (°C):</label>";
    html += "<input type='text' inputmode='decimal' pattern='[0-9]*[.]?[0-9]*' name='min_tank' value='" + String(config.min_tank_temp, 1) + "'></div>";
    html += "<div class='form-group'><label>Min Out Pipe Temp (°C):</label>";
    html += "<input type='text' inputmode='decimal' pattern='[0-9]*[.]?[0-9]*' name='min_out' value='" + String(config.min_out_pipe_temp, 1) + "'></div>";
    html += "<div class='form-group'><label>Poll Interval (seconds):</label>";
    html += "<input type='number' name='poll_interval' value='" + String(config.poll_interval) + "' min='5' max='300'></div>";
    html += "</div>";
    
    html += "<div class='section'>";
    html += "<h2>🔆 Display Settings</h2>";
    html += "<div class='form-group'><label>Screen Brightness (0-255):</label>";
    html += "<input type='number' name='brightness' value='" + String(config.screen_brightness) + "' min='0' max='255'></div>";
    html += "</div>";
    
    html += "<button type='submit' class='btn'>💾 Save Configuration</button>";
    html += "</form>";
    html += "</div>";
    
    // JavaScript for HA integration
    html += "<script>";
    html += "function testHA(){";
    html += "  var status=document.getElementById('ha-status');";
    html += "  status.className='status loading';status.innerHTML='Testing connection...';";
    html += "  var url=document.getElementById('ha_url').value;";
    html += "  var token=document.getElementById('ha_token').value;";
    html += "  var formData=new FormData();formData.append('ha_url',url);formData.append('ha_token',token);";
    html += "  fetch('/ha/test',{method:'POST',body:formData})";
    html += "  .then(r=>r.json()).then(d=>{";
    html += "    status.className='status '+(d.success?'success':'error');";
    html += "    status.innerHTML=d.success?'✅ Connected to Home Assistant':'❌ '+d.error;";
    html += "  }).catch(e=>{status.className='status error';status.innerHTML='❌ Network error';});";
    html += "}";
    html += "function loadEntities(){";
    html += "  var status=document.getElementById('ha-status');";
    html += "  status.className='status loading';status.innerHTML='Loading sensors...';";
    html += "  var url=document.getElementById('ha_url').value;";
    html += "  var token=document.getElementById('ha_token').value;";
    html += "  var formData=new FormData();formData.append('ha_url',url);formData.append('ha_token',token);";
    html += "  fetch('/ha/entities',{method:'POST',body:formData}).then(r=>r.json()).then(d=>{";
    html += "    if(d.error){status.className='status error';status.innerHTML='❌ '+d.error;return;}";
    html += "    status.className='status success';status.innerHTML='✅ Found '+d.entities.length+' temperature sensors';";
    html += "    var selects=['entity_tank','entity_out','entity_heat_in','entity_room'];";
    html += "    selects.forEach(id=>{";
    html += "      var sel=document.getElementById(id);";
    html += "      var cur=sel.value;";
    html += "      sel.innerHTML='<option value=\"\">-- Select sensor --</option>';";
    html += "      d.entities.forEach(e=>{";
    html += "        var opt=document.createElement('option');";
    html += "        opt.value=e.id;opt.text=e.name+' ('+e.state+e.unit+')';";
    html += "        if(e.id===cur)opt.selected=true;";
    html += "        sel.appendChild(opt);";
    html += "      });";
    html += "    });";
    html += "  }).catch(e=>{status.className='status error';status.innerHTML='❌ Network error';});";
    html += "}";
    html += "function testDisplay(){";
    html += "  var status=document.getElementById('ha-status');";
    html += "  status.className='status loading';status.innerHTML='🎨 Testing display modes...';";
    html += "  fetch('/display-test').then(r=>r.json()).then(d=>{";
    html += "    status.className='status success';status.innerHTML='✅ '+d.message;";
    html += "  }).catch(e=>{status.className='status error';status.innerHTML='❌ Error';});";
    html += "}";
    html += "function updateTemps(){";
    html += "  fetch('/status').then(r=>r.json()).then(d=>{";
    html += "    document.getElementById('t-room').innerText=d.roomTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-tank').innerText=d.tankTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-out').innerText=d.outPipeTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-hin').innerText=d.heatingInTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('ha-conn').innerHTML='<span style=\"color:#4CAF50\">● Live</span>';";
    html += "  }).catch(e=>{";
    html += "    document.getElementById('ha-conn').innerHTML='<span style=\"color:#f44336\">● Error</span>';";
    html += "  });";
    html += "}";
    html += "updateTemps();setInterval(updateTemps,5000);";
    html += "</script>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleSaveConfig() {
    Config config = configManager.getConfig();
    
    // Update Home Assistant settings
    server.arg("ha_url").toCharArray(config.ha_url, sizeof(config.ha_url));
    server.arg("ha_token").toCharArray(config.ha_token, sizeof(config.ha_token));
    
    // Update entity IDs
    server.arg("entity_tank").toCharArray(config.entity_tank_temp, sizeof(config.entity_tank_temp));
    server.arg("entity_out").toCharArray(config.entity_out_pipe_temp, sizeof(config.entity_out_pipe_temp));
    server.arg("entity_heat_in").toCharArray(config.entity_heating_in_temp, sizeof(config.entity_heating_in_temp));
    server.arg("entity_room").toCharArray(config.entity_room_temp, sizeof(config.entity_room_temp));
    
    // Update thresholds and settings with validation
    float minTank = server.arg("min_tank").toFloat();
    float minOut = server.arg("min_out").toFloat();
    
    // Validate temperature thresholds
    if (minTank < 0.0 || minTank > 100.0) {
        server.send(400, "text/html", "<html><body><h1>Error: Invalid tank threshold (0-100°C)</h1></body></html>");
        return;
    }
    if (minOut < 0.0 || minOut > 100.0) {
        server.send(400, "text/html", "<html><body><h1>Error: Invalid out pipe threshold (0-100°C)</h1></body></html>");
        return;
    }
    
    config.min_tank_temp = minTank;
    config.min_out_pipe_temp = minOut;
    
    config.poll_interval = server.arg("poll_interval").toInt();
    if (config.poll_interval < 5) config.poll_interval = 5;
    if (config.poll_interval > 300) config.poll_interval = 300;
    
    // Update display settings
    config.screen_brightness = server.arg("brightness").toInt();
    if (config.screen_brightness < 0) config.screen_brightness = 0;
    if (config.screen_brightness > 255) config.screen_brightness = 255;
    
    // Save to NVS
    configManager.setHA(config.ha_url, config.ha_token);
    configManager.setEntities(config.entity_tank_temp, config.entity_out_pipe_temp, 
                              config.entity_heating_in_temp,
                              config.entity_room_temp);
    configManager.setThresholds(config.min_tank_temp, config.min_out_pipe_temp);
    configManager.setBrightness(config.screen_brightness);
    configManager.save();
    
    // Apply changes immediately without reboot
    display.setBrightness(config.screen_brightness);
    display.setThresholds(config.min_tank_temp, config.min_out_pipe_temp);
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta http-equiv='refresh' content='2;url=/'>";
    html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#667eea;color:#fff;}</style>";
    html += "</head><body>";
    html += "<h1>✅ Configuration Saved!</h1>";
    html += "<p>Settings applied successfully.</p>";
    html += "<p>Redirecting back to config page...</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
}

void handleStatus() {
    Serial.println("Status request - sending temps:");
    Serial.print("  roomTemp="); Serial.println(roomTemp);
    Serial.print("  tankTemp="); Serial.println(tankTemp);
    Serial.print("  outPipeTemp="); Serial.println(outPipeTemp);
    
    String json = "{";
    json += "\"roomTemp\":" + String(roomTemp, 1) + ",";
    json += "\"tankTemp\":" + String(tankTemp, 1) + ",";
    json += "\"outPipeTemp\":" + String(outPipeTemp, 1) + ",";
    json += "\"heatingInTemp\":" + String(heatingInTemp, 1) + ",";
    json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false") + ",";
    json += "\"haConnected\":" + String(haConnected ? "true" : "false");
    json += "}";
    
    server.send(200, "application/json", json);
}

void handleDisplayTest() {
    testMode = !testMode;
    testState = 0;
    lastTestStateChange = millis();
    
    String message = testMode ? "Display test started - cycling through states every 3s" : "Display test stopped";
    String json = "{\"success\":true,\"message\":\"" + message + "\"}";
    
    server.send(200, "application/json", json);
    
    // When exiting test mode, immediately return to production
    if (!testMode) {
        // Reset heating state (will be detected properly on next poll)
        heatingActive = false;
        
        // Force immediate HA poll to get real sensor data
        pollHomeAssistant();
        
        // Force display update with actual data
        display.updateBathStatus(bathIsReady);
        display.updateHeatingStatus(heatingActive);
        
        // Force immediate display refresh to clear test state
        display.refresh();
        
        Serial.println("Test mode stopped - resumed production operation");
    }
}

