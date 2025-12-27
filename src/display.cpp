#include "display.h"
#include "baby_bath_image.h"

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
// #include <LittleFS.h> // Commented out - causing compilation issues with LovyanGFX on ESP32-C6

// Pin definitions for Waveshare ESP32-C6 1.47"
#define TFT_BL 22  // Backlight pin

// Display timing constants
const unsigned long BATH_IMAGE_DISPLAY_TIME = 4000;   // 4 seconds
const unsigned long ROOM_TEMP_DISPLAY_TIME = 2000;    // 2 seconds
const unsigned long ACTIVITY_TIMEOUT = 120000;        // 2 minutes

// LovyanGFX configuration for Waveshare ESP32-C6 1.47"
class LGFX_ESP32C6 : public lgfx::LGFX_Device
{
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI _bus_instance;
public:
    LGFX_ESP32C6(void)
    {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 80000000;
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = 7;
            cfg.pin_mosi = 6;
            cfg.pin_miso = 5;
            cfg.pin_dc = 15;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }

        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs = 14;
            cfg.pin_rst = 21;
            cfg.pin_busy = -1;
            cfg.panel_width = 172;
            cfg.panel_height = 320;
            cfg.offset_x = 34;
            cfg.offset_y = 0;
            cfg.offset_rotation = 0;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }
};

static LGFX_ESP32C6 tft;

DisplayManager::DisplayManager() {
    tempData.tankTemp = 0;
    tempData.outPipeTemp = 0;
    tempData.heatingInTemp = 0;
    tempData.roomTemp = 0;
    tempData.previousTankTemp = 0;
    tempData.tankValid = false;
    tempData.outPipeValid = false;
    tempData.heatingInValid = false;
    tempData.roomValid = false;
    tempData.heatingActive = false;
    tempData.tankDropping = false;
    tempData.lastUpdate = 0;
    tempData.lastTankUpdate = 0;
    tempData.lastHotWaterActivity = 0;
    bathReady = false;
    showingBathStatus = false;
    showingBathImage = true;  // Start with bath image
    lastDisplayToggle = 0;
    previousBathReady = false;
    minTankTemp = 52.0;
    minOutPipeTemp = 38.0;
    useCelsius = true;
    needsRedraw = true;
}

void DisplayManager::begin(int brightness) {
    Serial.println("Initializing display...");
    tft.init();
    Serial.println("Display init done");
    
    tft.setRotation(1);  // Horizontal mode (landscape)
    tft.fillScreen(TFT_BLACK);
    
    setBrightness(brightness);
    
    // LittleFS temporarily disabled due to LovyanGFX compilation issues
    // Will re-enable once library compatibility is fixed
    /*
    // Initialize LittleFS for image loading
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    } else {
        Serial.println("LittleFS mounted successfully");
    }
    */
    
    Serial.println("Display ready");
}

void DisplayManager::setBrightness(int brightness) {
    // Use PWM for backlight control (0-255)
    // Clamp brightness to safe range
    if (brightness < 0) brightness = 0;
    if (brightness > 255) brightness = 255;
    
    ledcAttach(TFT_BL, 5000, 8);  // 5kHz, 8-bit resolution
    ledcWrite(TFT_BL, brightness);
    Serial.printf("Backlight set to %d\n", brightness);
}

void DisplayManager::setTemperatureUnit(bool celsius) {
    useCelsius = celsius;
}

void DisplayManager::setThresholds(float minTank, float minOutPipe) {
    minTankTemp = minTank;
    minOutPipeTemp = minOutPipe;
}

float DisplayManager::convertTemp(float temp) {
    if (useCelsius) {
        return temp;
    }
    return (temp * 9.0 / 5.0) + 32.0;  // Convert to Fahrenheit
}

const char* DisplayManager::getTempUnit() {
    return useCelsius ? "C" : "F";
}

void DisplayManager::drawHeader() {
    // Draw room temperature in very large font at center
    tft.setTextDatum(MC_DATUM);
    if (tempData.roomValid) {
        char buf[20];
        snprintf(buf, sizeof(buf), "%.1f%s", convertTemp(tempData.roomTemp), getTempUnit());
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(buf, 160, 86, 7);  // Font 7 is largest
    } else {
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("--", 160, 86, 7);
    }
}

void DisplayManager::drawTemperatures() {
    // Don't show temperature bars when bath status is being displayed
    // Temperature info only shown when no bath decision yet
}

void DisplayManager::drawRoomTemperature() {
    // Display large room temperature filling the screen
    int centerX = 160;
    int centerY = 86;
    
    tft.fillScreen(TFT_BLACK);
    
    if (tempData.roomValid) {
        // Draw extra large temperature - use setTextSize to scale up font 7
        char buf[16];
        float displayTemp = convertTemp(tempData.roomTemp);
        snprintf(buf, sizeof(buf), "%.1f%s", displayTemp, getTempUnit());
        
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);  // Double the size of font 7
        tft.drawString(buf, centerX, centerY - 10, 7);
        tft.setTextSize(1);  // Reset
        
        // Label - bigger
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString("Room", centerX, centerY + 65, 4);
        tft.setTextSize(1);
    } else {
        // No room temp data yet
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.drawString("Waiting...", centerX, centerY, 4);
        tft.setTextSize(1);
    }
}

void DisplayManager::drawHeatingIndicator() {
    // Draw smooth wavy lines simulating heat waves rising
    // Much cleaner and more elegant animation
    int offset = (millis() / 60) % 172;  // Smooth upward scroll
    
    // Draw wavy heat lines on left side
    for (int wave = 0; wave < 3; wave++) {
        int startY = (wave * 60 + offset) % (172 + 60) - 60;
        
        for (int y = startY; y < startY + 50; y += 2) {
            if (y >= 0 && y < 172) {
                // Create sine wave effect for horizontal position
                int amplitude = 6;
                int x = amplitude * sin((y + wave * 20) * 0.2) + amplitude + 2;
                
                // Color fades as it rises (older waves are dimmer)
                uint16_t color;
                float fade = (float)(y - startY) / 50.0;
                if (fade < 0.33) color = TFT_RED;
                else if (fade < 0.66) color = TFT_ORANGE;
                else color = TFT_YELLOW;
                
                // Draw thick wavy line
                tft.fillCircle(x, y, 3, color);
            }
        }
    }
    
    // Draw wavy heat lines on right side (mirrored)
    for (int wave = 0; wave < 3; wave++) {
        int startY = (wave * 60 + offset) % (172 + 60) - 60;
        
        for (int y = startY; y < startY + 50; y += 2) {
            if (y >= 0 && y < 172) {
                int amplitude = 6;
                int x = 320 - (amplitude * sin((y + wave * 20) * 0.2) + amplitude + 2);
                
                uint16_t color;
                float fade = (float)(y - startY) / 50.0;
                if (fade < 0.33) color = TFT_RED;
                else if (fade < 0.66) color = TFT_ORANGE;
                else color = TFT_YELLOW;
                
                tft.fillCircle(x, y, 3, color);
            }
        }
    }
}

void DisplayManager::drawStatus() {
    // Draw large icon in center top area (landscape: 320x172)
    int centerX = 160;
    int centerY = 86;  // Center vertically (172/2)
    
    if (bathReady) {
        if (showingBathImage) {
            // Display baby bath image from PROGMEM
            tft.fillScreen(TFT_BLACK);
            
            // Image is 172x172, center it on screen (320x172)
            int imageX = (320 - baby_bath_image_width) / 2;
            int imageY = 0;
            tft.pushImage(imageX, imageY, baby_bath_image_width, baby_bath_image_height, baby_bath_image);
            
            // Draw heating indicator - animated heat waves on sides
            if (tempData.heatingActive) {
                drawHeatingIndicator();
            }
        } else {
            // Show room temperature
            drawRoomTemperature();
            
            // Draw heating indicator on room temp screen too
            if (tempData.heatingActive) {
                drawHeatingIndicator();
            }
        }
    } else {
        // Draw large octagon stop sign centered on screen
        // Screen is 320x172, make circle smaller to fit with margin
        int size = 80;  // Reduced radius to prevent bottom cutoff
        int stopY = 86;  // Exact vertical center (172/2)
        tft.fillCircle(centerX, stopY, size, TFT_RED);
        tft.drawCircle(centerX, stopY, size, TFT_WHITE);
        tft.drawCircle(centerX, stopY, size + 1, TFT_WHITE);
        tft.drawCircle(centerX, stopY, size + 2, TFT_WHITE);
        
        // Draw STOP text - font 4 with proper centering
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.setTextDatum(MC_DATUM);  // Middle center alignment
        tft.setTextSize(2);  // Double size
        tft.drawString("STOP", centerX, stopY, 4);  // Use stopY (86) for perfect centering
        tft.setTextSize(1);  // Reset
    }
}

void DisplayManager::updateTemperature(int sensor, float value) {
    unsigned long now = millis();
    bool changed = false;
    bool wasBathReady = bathReady;
    
    switch (sensor) {
        case 0: // Tank
            // Check for significant change
            if (abs(tempData.tankTemp - value) > 0.1) {
                changed = true;
                tempData.lastHotWaterActivity = now;  // Activity detected
            }
            
            // Detect if tank temperature is dropping (water is flowing)
            if (tempData.tankValid && now - tempData.lastTankUpdate > 10000) { // Check every 10+ seconds
                float tempDiff = value - tempData.previousTankTemp;
                // Tank is dropping if temp decreased by more than 0.3°C
                if (tempDiff < -0.3) {
                    tempData.tankDropping = true;
                    changed = true;
                    tempData.lastHotWaterActivity = now;  // Activity detected
                } else if (tempDiff > 0.5) {
                    // Tank is heating up again, reset dropping state
                    tempData.tankDropping = false;
                    changed = true;
                    tempData.lastHotWaterActivity = now;  // Activity detected
                }
                // If change is minimal, maintain previous state
            }
            
            tempData.previousTankTemp = tempData.tankTemp;
            tempData.tankTemp = value;
            tempData.tankValid = true;
            tempData.lastTankUpdate = now;
            break;
        case 1: // Out Pipe
            if (abs(tempData.outPipeTemp - value) > 0.1) {
                changed = true;
                tempData.lastHotWaterActivity = now;  // Activity detected
            }
            tempData.outPipeTemp = value;
            tempData.outPipeValid = true;
            break;
        case 2: // Heating In
            if (abs(tempData.heatingInTemp - value) > 0.1) {
                changed = true;
                tempData.lastHotWaterActivity = now;  // Activity detected
            }
            tempData.heatingInTemp = value;
            tempData.heatingInValid = true;
            break;
        case 3: // Room
            if (abs(tempData.roomTemp - value) > 0.1) changed = true;
            tempData.roomTemp = value;
            tempData.roomValid = true;
            break;
    }
    tempData.lastUpdate = now;
    
    // Smart bath readiness logic:
    // Bath is ready when:
    // 1. Tank is hot enough (≥ minTankTemp, default 52°C)
    // 2. Tank temperature is dropping (water is flowing) OR was already ready
    // 3. Out pipe is warm enough (≥ minOutPipeTemp, default 38°C)
    if (tempData.tankValid && tempData.outPipeValid) {
        bool tankHot = tempData.tankTemp >= minTankTemp;
        bool pipeWarm = tempData.outPipeTemp >= minOutPipeTemp;
        bool waterFlowing = tempData.tankDropping;
        
        // Once bath becomes ready, it stays ready until tank drops below threshold
        // or pipe cools down significantly
        if (bathReady) {
            // Stay ready unless tank gets too cold or pipe drops too much
            bathReady = tankHot && tempData.outPipeTemp >= (minOutPipeTemp - 2.0);
        } else {
            // Become ready when tank is hot, water is flowing, and pipe is warm
            bathReady = tankHot && waterFlowing && pipeWarm;
        }
    } else {
        bathReady = false;
    }
    
    // Determine which display mode to show
    // Show bath status only if there's been hot water activity in last 2 minutes
    bool shouldShowBathStatus = (now - tempData.lastHotWaterActivity) < ACTIVITY_TIMEOUT;
    
    // Mark display for redraw if:
    // - Temperature changed
    // - Bath status changed
    // - Display mode needs to change
    if (changed || bathReady != wasBathReady || shouldShowBathStatus != showingBathStatus) {
        showingBathStatus = shouldShowBathStatus;
        needsRedraw = true;
    }
}

void DisplayManager::updateBathStatus(bool ready) {
    if (ready && !bathReady) {
        // Just became ready - reset toggle to show bath image first
        showingBathImage = true;
        lastDisplayToggle = millis();
    }
    if (bathReady != ready) {
        needsRedraw = true;
    }
    bathReady = ready;
}

void DisplayManager::updateHeatingStatus(bool active) {
    if (tempData.heatingActive != active) {
        needsRedraw = true;
    }
    tempData.heatingActive = active;
}

void DisplayManager::showConfigMode() {
    tft.fillScreen(TFT_NAVY);
    
    // For horizontal display (320x172)
    int centerX = 160;
    int centerY = 70;
    
    // Draw gear icon
    tft.fillCircle(centerX, centerY, 40, TFT_ORANGE);
    tft.fillCircle(centerX, centerY, 25, TFT_NAVY);
    
    // Draw config text at bottom
    tft.setTextColor(TFT_WHITE, TFT_NAVY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("CONFIG MODE", centerX, 128, 4);
    tft.setTextDatum(BC_DATUM);
    tft.drawString("Connect to: Water-Status-AP", centerX, 165, 2);
}

void DisplayManager::showIPAddress(IPAddress ip) {
    tft.fillScreen(TFT_DARKGREEN);
    
    int centerX = 160;
    int centerY = 70;
    
    // Draw checkmark circle
    tft.fillCircle(centerX, centerY, 40, TFT_GREEN);
    tft.fillCircle(centerX, centerY, 35, TFT_DARKGREEN);
    
    // Draw checkmark
    tft.drawLine(centerX - 15, centerY, centerX - 5, centerY + 15, TFT_GREEN);
    tft.drawLine(centerX - 5, centerY + 15, centerX + 15, centerY - 10, TFT_GREEN);
    tft.drawLine(centerX - 15, centerY + 1, centerX - 5, centerY + 16, TFT_GREEN);
    tft.drawLine(centerX - 5, centerY + 16, centerX + 15, centerY - 9, TFT_GREEN);
    tft.drawLine(centerX - 15, centerY - 1, centerX - 5, centerY + 14, TFT_GREEN);
    tft.drawLine(centerX - 5, centerY + 14, centerX + 15, centerY - 11, TFT_GREEN);
    
    // Display IP address
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi Connected!", centerX, 128, 4);
    
    char ipStr[20];
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(ipStr, centerX, 165, 4);
}

void DisplayManager::showStartupScreen(IPAddress ip) {
    tft.fillScreen(TFT_DARKGREEN);
    
    int centerX = 160;
    
    // Draw checkmark circle
    tft.fillCircle(centerX, 60, 40, TFT_GREEN);
    tft.drawCircle(centerX, 60, 40, TFT_WHITE);
    
    // Draw checkmark
    tft.drawLine(centerX - 15, 60, centerX - 5, 75, TFT_WHITE);
    tft.drawLine(centerX - 5, 75, centerX + 15, 50, TFT_WHITE);
    tft.drawLine(centerX - 15, 61, centerX - 5, 76, TFT_WHITE);
    tft.drawLine(centerX - 5, 76, centerX + 15, 51, TFT_WHITE);
    
    // Display IP address
    tft.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("WiFi Connected!", centerX, 120, 4);
    
    char ipStr[20];
    sprintf(ipStr, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(ipStr, centerX, 160, 4);
}

void DisplayManager::refresh() {
    // Check if we need to toggle bath/room display when bath is ready
    if (bathReady && showingBathStatus) {
        unsigned long now = millis();
        unsigned long toggleInterval = showingBathImage ? BATH_IMAGE_DISPLAY_TIME : ROOM_TEMP_DISPLAY_TIME;
        
        if (now - lastDisplayToggle > toggleInterval) {
            lastDisplayToggle = now;
            showingBathImage = !showingBathImage;
            needsRedraw = true;
        }
    }
    
    // Only redraw if something changed (optimization)
    if (!needsRedraw) {
        return;
    }
    
    tft.fillScreen(TFT_BLACK);
    drawHeader();
    
    // Show bath status only if there's recent hot water activity
    // Otherwise show room temperature
    if (showingBathStatus) {
        drawStatus();
    } else {
        drawRoomTemperature();
    }
    
    needsRedraw = false;
}

void DisplayManager::drawBathtubIcon(int x, int y, int size, bool ready) {
    // Draw a simple but clear bathtub icon
    int tileWidth = size;
    int tileHeight = size * 0.6;
    
    uint16_t color = ready ? TFT_WHITE : TFT_DARKGREY;
    uint16_t waterColor = ready ? TFT_CYAN : TFT_BLUE;
    
    // Draw bathtub body (rounded rectangle)
    int tubX = x - tileWidth/2;
    int tubY = y - tileHeight/4;
    
    tft.fillRoundRect(tubX, tubY, tileWidth, tileHeight, 8, color);
    
    // Draw water inside (if ready, make it animated-looking)
    if (ready) {
        tft.fillRoundRect(tubX + 4, tubY + 8, tileWidth - 8, tileHeight - 16, 5, waterColor);
    }
    
    // Draw legs
    int legWidth = 6;
    int legHeight = size * 0.25;
    tft.fillRect(tubX + 10, tubY + tileHeight - 2, legWidth, legHeight, color);
    tft.fillRect(tubX + tileWidth - 16, tubY + tileHeight - 2, legWidth, legHeight, color);
    
    // Draw checkmark or X overlay
    if (ready) {
        // Draw big checkmark
        int checkX = x - 8;
        int checkY = y + 8;
        tft.fillTriangle(checkX - 10, checkY, checkX - 5, checkY + 10, checkX - 8, checkY + 7, TFT_GREEN);
        tft.fillTriangle(checkX - 5, checkY + 10, checkX + 15, checkY - 15, checkX + 12, checkY - 18, TFT_GREEN);
    } else {
        // Draw big X
        int crossSize = 20;
        int crossThick = 6;
        // Draw X as two thick lines
        for(int i = 0; i < crossThick; i++) {
            tft.drawLine(x - crossSize + i, y - crossSize, x + crossSize + i, y + crossSize, TFT_RED);
            tft.drawLine(x + crossSize + i, y - crossSize, x - crossSize + i, y + crossSize, TFT_RED);
        }
    }
}

void DisplayManager::drawThermometerBar(int x, int y, int width, int height, 
                                        float temp, float minTemp, float maxTemp, uint16_t color) {
    // Draw border
    tft.drawRect(x, y, width, height, TFT_WHITE);
    
    // Calculate fill percentage
    float percent = (temp - minTemp) / (maxTemp - minTemp);
    if (percent < 0) percent = 0;
    if (percent > 1) percent = 1;
    
    // Draw background
    tft.fillRect(x + 2, y + 2, width - 4, height - 4, TFT_BLACK);
    
    // Draw filled portion (from bottom up like mercury)
    int fillHeight = (height - 4) * percent;
    int fillY = y + height - 2 - fillHeight;
    
    // Color intensity based on temperature
    uint16_t fillColor = color;
    if (percent < 0.3) {
        fillColor = TFT_BLUE; // Cold
    } else if (percent > 0.7) {
        fillColor = TFT_RED; // Hot
    }
    
    tft.fillRect(x + 2, fillY, width - 4, fillHeight, fillColor);
    
    // Draw temperature number on top
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f", convertTemp(temp));
    tft.setTextColor(TFT_WHITE, color);
    tft.setTextDatum(MC_DATUM);
    tft.setTextSize(1);
    
    // Draw colored background for number
    int textWidth = strlen(buf) * 12;
    tft.fillRoundRect(x + width/2 - textWidth/2 - 2, y + height/2 - 12, 
                     textWidth + 4, 24, 3, color);
    tft.drawString(buf, x + width/2, y + height/2, 4);
}
