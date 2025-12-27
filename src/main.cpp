#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"
#include "display.h"
#include "web_interface.h"

// RGB LED on Waveshare ESP32-C6 1.47" LCD
#define RGB_LED_PIN 8
#define NUM_LEDS 1
Adafruit_NeoPixel rgbLed(NUM_LEDS, RGB_LED_PIN, NEO_RGB + NEO_KHZ800);

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
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;

// Sensor temperatures
float tankTemp = 0.0;
float outPipeTemp = 0.0;
float heatingInTemp = 0.0;
float heatingOutTemp = 0.0;
float roomTemp = 0.0;

// LED state for STOP flashing
bool bathIsReady = false;
unsigned long lastLedFlash = 0;
bool ledOn = false;

// Function declarations
void setupWiFi();
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
    rgbLed.setBrightness(50);
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
    
    // Handle web server
    if (wifiConnected) {
        server.handleClient();
    }
    
    // Poll Home Assistant periodically
    if (wifiConnected) {
        Config config = configManager.getConfig();
        unsigned long pollInterval = config.poll_interval * 1000UL;
        unsigned long now = millis();
        
        if (now - lastHAPoll > pollInterval) {
            lastHAPoll = now;
            pollHomeAssistant();
        }
    }
    
    // Update display periodically
    unsigned long now = millis();
    if (now - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
        lastDisplayUpdate = now;
        display.refresh();
    }
    
    // Flash LED red when STOP (not ready)
    if (!bathIsReady) {
        if (now - lastLedFlash > 500) {  // Flash every 500ms
            lastLedFlash = now;
            ledOn = !ledOn;
            if (ledOn) {
                rgbLed.setPixelColor(0, rgbLed.Color(255, 0, 0));  // Red
            } else {
                rgbLed.clear();
            }
            rgbLed.show();
        }
    } else {
        // Bath ready - LED off or green
        if (ledOn) {
            rgbLed.clear();
            rgbLed.show();
            ledOn = false;
        }
    }
    
    delay(100);
}

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

// Fetch temperature from a single Home Assistant entity
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
    http.setTimeout(5000);
    
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

// Poll all configured entities from Home Assistant
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
    
    // Fetch heating out temperature
    if (strlen(config.entity_heating_out_temp) > 0) {
        float temp = fetchHAEntityState(config.entity_heating_out_temp);
        if (temp != 0.0 || heatingOutTemp == 0.0) {
            heatingOutTemp = temp;
            display.updateTemperature(3, heatingOutTemp);
            anySuccess = true;
        }
    }
    
    // Fetch room temperature
    if (strlen(config.entity_room_temp) > 0) {
        float temp = fetchHAEntityState(config.entity_room_temp);
        if (temp != 0.0 || roomTemp == 0.0) {
            roomTemp = temp;
            display.updateTemperature(4, roomTemp);
            anySuccess = true;
        }
    }
    
    haConnected = anySuccess;
    
    Serial.print("Any success: ");
    Serial.println(anySuccess ? "YES" : "NO");
    
    // Check bath readiness and update global state for LED flashing
    bathIsReady = (tankTemp >= config.min_tank_temp && outPipeTemp >= config.min_out_pipe_temp);
    display.updateBathStatus(bathIsReady);
    Serial.print("Bath ready: ");
    Serial.println(bathIsReady ? "YES" : "NO");
    Serial.print("  Tank: "); Serial.print(tankTemp); Serial.print(" >= "); Serial.println(config.min_tank_temp);
    Serial.print("  OutPipe: "); Serial.print(outPipeTemp); Serial.print(" >= "); Serial.println(config.min_out_pipe_temp);
}

// Fetch list of temperature sensors from Home Assistant
void handleHAEntities() {
    Config config = configManager.getConfig();
    
    String ha_url = server.arg("ha_url");
    String ha_token = server.arg("ha_token");
    
    // Use provided params or fall back to saved config
    if (ha_url.length() == 0) ha_url = config.ha_url;
    if (ha_token.length() == 0) ha_token = config.ha_token;
    
    if (ha_url.length() == 0 || ha_token.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"HA not configured\"}");
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
    http.setTimeout(15000);
    
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
        http.setTimeout(15000);
        
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
    
    // Use /api/ endpoint which returns API info
    String url = ha_url + "/api/";
    
    http.begin(url);
    http.addHeader("Authorization", String("Bearer ") + ha_token);
    http.setTimeout(5000);
    
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

void startWebServer() {
    // Setup web server routes for normal WiFi mode
    server.on("/", handleConfig);
    server.on("/save", HTTP_POST, handleSaveConfig);
    server.on("/status", handleStatus);
    server.on("/ha/entities", handleHAEntities);
    server.on("/ha/test", handleHATest);
    server.begin();
    Serial.println("Web server started on port 80");
    Serial.print("Access at: http://");
    Serial.println(WiFi.localIP());
}

void handleConfig() {
    Config config = configManager.getConfig();
    
    String html = "<!DOCTYPE html><html><head>";
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
    html += "<div class='temp-display'>Heating Out: <span class='temp-value' id='t-hout'>" + String(heatingOutTemp, 1) + "°C</span></div>";
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
    
    // Heating out temp
    html += "<div class='form-group'><label>Heating Out Temperature:</label>";
    html += "<select name='entity_heat_out' id='entity_heat_out'><option value=''>-- Select sensor --</option>";
    if (strlen(config.entity_heating_out_temp) > 0) {
        html += "<option value='" + String(config.entity_heating_out_temp) + "' selected>" + String(config.entity_heating_out_temp) + "</option>";
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
    html += "<input type='number' step='0.1' name='min_tank' value='" + String(config.min_tank_temp, 1) + "'></div>";
    html += "<div class='form-group'><label>Min Out Pipe Temp (°C):</label>";
    html += "<input type='number' step='0.1' name='min_out' value='" + String(config.min_out_pipe_temp, 1) + "'></div>";
    html += "<div class='form-group'><label>Poll Interval (seconds):</label>";
    html += "<input type='number' name='poll_interval' value='" + String(config.poll_interval) + "' min='5' max='300'></div>";
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
    html += "  fetch('/ha/test?ha_url='+encodeURIComponent(url)+'&ha_token='+encodeURIComponent(token))";
    html += "  .then(r=>r.json()).then(d=>{";
    html += "    status.className='status '+(d.success?'success':'error');";
    html += "    status.innerHTML=d.success?'✅ Connected to Home Assistant':'❌ '+d.error;";
    html += "  }).catch(e=>{status.className='status error';status.innerHTML='❌ Network error';});";
    html += "}";
    html += "function loadEntities(){";
    html += "  var status=document.getElementById('ha-status');";
    html += "  status.className='status loading';status.innerHTML='Loading sensors...';";
    html += "  fetch('/ha/entities').then(r=>r.json()).then(d=>{";
    html += "    if(d.error){status.className='status error';status.innerHTML='❌ '+d.error;return;}";
    html += "    status.className='status success';status.innerHTML='✅ Found '+d.entities.length+' temperature sensors';";
    html += "    var selects=['entity_tank','entity_out','entity_heat_in','entity_heat_out','entity_room'];";
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
    html += "function updateTemps(){";
    html += "  fetch('/status').then(r=>r.json()).then(d=>{";
    html += "    document.getElementById('t-room').innerText=d.roomTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-tank').innerText=d.tankTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-out').innerText=d.outPipeTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-hin').innerText=d.heatingInTemp.toFixed(1)+'°C';";
    html += "    document.getElementById('t-hout').innerText=d.heatingOutTemp.toFixed(1)+'°C';";
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
    server.arg("entity_heat_out").toCharArray(config.entity_heating_out_temp, sizeof(config.entity_heating_out_temp));
    server.arg("entity_room").toCharArray(config.entity_room_temp, sizeof(config.entity_room_temp));
    
    // Update thresholds and settings
    config.min_tank_temp = server.arg("min_tank").toFloat();
    config.min_out_pipe_temp = server.arg("min_out").toFloat();
    config.poll_interval = server.arg("poll_interval").toInt();
    if (config.poll_interval < 5) config.poll_interval = 5;
    if (config.poll_interval > 300) config.poll_interval = 300;
    
    // Save to NVS
    configManager.setHA(config.ha_url, config.ha_token);
    configManager.setEntities(config.entity_tank_temp, config.entity_out_pipe_temp, 
                              config.entity_heating_in_temp, config.entity_heating_out_temp,
                              config.entity_room_temp);
    configManager.setThresholds(config.min_tank_temp, config.min_out_pipe_temp);
    configManager.save();
    
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:#667eea;color:#fff;}</style>";
    html += "</head><body>";
    html += "<h1>✅ Configuration Saved!</h1>";
    html += "<p>Device will restart in 3 seconds...</p>";
    html += "<p>Reconnect to: http://" + WiFi.localIP().toString() + "</p>";
    html += "</body></html>";
    
    server.send(200, "text/html", html);
    
    delay(3000);
    ESP.restart();
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
    json += "\"heatingOutTemp\":" + String(heatingOutTemp, 1) + ",";
    json += "\"wifiConnected\":" + String(wifiConnected ? "true" : "false") + ",";
    json += "\"haConnected\":" + String(haConnected ? "true" : "false");
    json += "}";
    
    server.send(200, "application/json", json);
}

