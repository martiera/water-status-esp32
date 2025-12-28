# Water Status ESP32-C6 Monitor

A sophisticated ESP32-C6 based water temperature monitoring system with visual display and Home Assistant integration using Waveshare ESP32-C6 1.47" Display (320×172).

## Features

- 📊 **4 Temperature Monitoring Points**:
  - Hot water storage tank
  - Hot water out pipe
  - Heating pipe incoming temperature
  - Room temperature

- 🎯 **Smart Bath Readiness Detection**: Flexible logic determines if water is ready based on configurable temperature thresholds

- 🔥 **Heating Activity Detection**: Automatically detects when heating system is active and displays animated indicators

- 🚦 **RGB LED Status Indicator**:
  - 🔴 Red Flash: Water not ready
  - 🟠 Orange Pulse: Heating active
  - 🟢 Green Solid: Bath ready
  - 🔵 Blue: OTA update in progress

- 📱 **Web Configuration Interface**: Easy setup through browser-based configuration panel

- 🏠 **Home Assistant REST API Integration**: Fetches sensor data directly from Home Assistant

- 📡 **OTA Updates**: Flash firmware remotely without USB cable

- 🖥️ **Animated Display**: 
  - STOP sign when not ready
  - Baby bath image when ready (alternating with room temperature)
  - Animated heating indicators on sides

- 💾 **Persistent Configuration**: All settings saved to ESP32 NVS flash memory

## Hardware Requirements

- **Waveshare ESP32-C6 1.47" Display Development Board** (320×172 resolution, ST7789)
- Built-in RGB LED (GPIO 8)
- USB-C cable for programming and power
- Home Assistant instance with REST API access

## Pin Configuration

Hardware configuration (Waveshare ESP32-C6 1.47"):

```
TFT_MOSI = GPIO 6
TFT_SCLK = GPIO 7
TFT_CS   = GPIO 14
TFT_DC   = GPIO 15
TFT_RST  = GPIO 21
TFT_BL   = GPIO 22 (PWM backlight control)
RGB_LED  = GPIO 8
```

## Software Setup

### 1. Install PlatformIO

- **VS Code**: Install the PlatformIO IDE extension
- **CLI**: Install PlatformIO Core

### 2. Clone and Build

```bash
git clone <repository-url>
cd water-status-esp32
pio run
```

### 3. Upload to Device

**Initial upload (USB):**
```bash
pio run --target upload
```

**Remote upload (OTA - after first USB upload):**
```bash
# Find device IP address (shown on display at startup or in serial monitor)
# Then upload via OTA:
pio run --target upload --upload-port 192.168.1.XXX
```

Or configure OTA in `platformio.ini`:
```ini
; Uncomment these lines for OTA:
upload_protocol = espota
upload_port = 192.168.1.XXX
upload_flags = 
    --port=3232
    --auth=water-status
```

Then simply run:
```bash
pio run --target upload
```

**OTA Credentials:**
- Port: 3232 (default)
- Password: `water-status`
- Hostname: `water-status-XXXXXX` (last 6 chars of MAC address)

### 4. Monitor Serial Output

```bash
pio device monitor --baud 115200
```

## Initial Configuration

### First Boot - AP Mode

1. On first boot (or if WiFi not configured), the device creates a WiFi access point:
   - **SSID**: `Water-Status-AP`
   - **No Password**

2. Connect to the AP and navigate to the captive portal (should open automatically) or go to: `http://192.168.4.1`

3. Select your WiFi network and enter password

4. Device will restart and connect to your network

### Home Assistant Configuration

Once connected to WiFi, access the web interface at the device's IP address (shown on display for 3 seconds at startup, or check your router/serial monitor).

Navigate to `http://<device-ip>/` to configure:

#### 1. Home Assistant Connection

- **HA URL**: Your Home Assistant URL (e.g., `http://homeassistant.local:8123` or `http://192.168.1.100:8123`)
- **Long-Lived Access Token**: Create in HA Profile → Long-Lived Access Tokens
- Click **Test Connection** to verify

#### 2. Temperature Sensors

Click **Load Sensors** to fetch all available temperature sensors from Home Assistant, then select:

- **Tank Temperature**: Hot water storage tank sensor
- **Out Pipe Temperature**: Hot water outlet sensor  
- **Heating In Temperature**: Heating system inlet sensor (for heating detection)
- **Room Temperature**: Room temperature sensor (for display)

#### 3. Temperature Thresholds

- **Min Tank Temp**: Minimum tank temperature for bath (default: 52.0°C)
- **Min Out Pipe Temp**: Minimum out pipe temperature (default: 38.0°C)
- **Poll Interval**: How often to fetch from HA in seconds (default: 10)

#### 4. Display Settings

- **Screen Brightness**: 0-255 (default: 80)

Click **Save Configuration** to apply changes (no reboot required).

## Home Assistant Setup

### Required Sensors

You need temperature sensors configured in Home Assistant. Example configuration:

```yaml
# Example: ESPHome temperature sensors
esphome:
  sensor:
    - platform: dallas
      name: "Tank Temperature"
      address: 0x1234567890abcdef
      
    - platform: dallas
      name: "Out Pipe Temperature"
      address: 0xfedcba0987654321
      
    - platform: dallas
      name: "Heating In Temperature"
      address: 0x1111222233334444

# Or any other temperature sensor platform
sensor:
  - platform: mqtt
    name: "Tank Temperature"
    state_topic: "home/sensors/tank_temp"
    unit_of_measurement: "°C"
    device_class: temperature
```

The device will automatically discover and list all temperature sensors with `device_class: temperature` when you click "Load Sensors".

## Display Interface

### Display Modes

**When Bath NOT Ready:**
- Large red STOP sign centered on screen
- LED flashes red every 500ms

**When Bath Ready:**
Alternates between:
- **Bath Image** (4 seconds): Baby bath PNG image
- **Room Temperature** (2 seconds): Large room temperature display

### Heating Indicator

When heating system is active (detected by temperature increase in heating inlet):
- Animated vertical bars appear on left and right edges
- Bars scroll upward in red, orange, and yellow colors
- Visible on both bath image and room temperature displays

### LED Status

- 🔴 **Red Flash** (500ms): Bath not ready
- 🟠 **Orange Pulse** (1s): Bath ready + heating active  
- 🟢 **Green Solid**: Bath ready + heating inactive

## Bath Readiness Logic

The system uses flexible logic to handle different scenarios:

```
Bath is ready when:
  (Out Pipe Temp >= Min Out Pipe Threshold)
  OR
  (Out Pipe Temp < Tank Temp AND Tank Temp >= Min Tank Threshold)
```

**Why this works:**
- If out pipe is already hot (≥38°C), bath is ready regardless of tank
- If out pipe is colder than tank, check if tank is hot enough (≥52°C)
- This handles both "already flowing" and "tank full but not yet flowing" scenarios

## Heating Detection

Heating activity is automatically detected:

- Monitors heating inlet temperature every 60 seconds
- Heating considered **ACTIVE** when temperature increases by >0.5°C per minute
- Heating considered **INACTIVE** when temperature drops by >1.0°C per minute
- Visual animated indicators appear on display when active

## Configuration Parameters

## Configuration Parameters

### API Endpoints

- **GET `/`**: Main configuration web interface
- **POST `/save`**: Save configuration (applies immediately, no reboot)
- **GET `/status`**: JSON API returning current sensor readings
- **POST `/ha/test`**: Test Home Assistant connection
- **POST `/ha/entities`**: Fetch available temperature sensors from HA
- **GET `/display-test`**: Toggle display test mode (cycles through 4 states)

### Status API Response

```json
{
  "roomTemp": 22.5,
  "tankTemp": 55.2,
  "outPipeTemp": 42.1,
  "heatingInTemp": 38.7,
  "wifiConnected": true,
  "haConnected": true
}
```

## Troubleshooting

## Troubleshooting

### Display Not Working

- Check hardware connections to Waveshare board
- Verify TFT_BL (GPIO 22) backlight pin
- Adjust brightness via web interface (0-255, default 80)

### WiFi Connection Failed

- Double-check SSID and password in captive portal
- Ensure 2.4GHz WiFi (ESP32-C6 doesn't support 5GHz)
- Check signal strength - device auto-reconnects every 30 seconds
- Look for "Water-Status-AP" if connection fails

### Home Assistant Not Connecting

- Use **Test Connection** button in web interface to verify URL and token
- Verify Home Assistant URL format: `http://homeassistant.local:8123` or `http://IP:8123`
- Check long-lived access token is valid (HA Profile → Long-Lived Access Tokens)
- Ensure firewall allows ESP32 to reach HA on port 8123
- Check HA logs for authentication errors

### No Temperature Updates

- Verify entity IDs are correct (use **Load Sensors** button)
- Check sensors have `device_class: temperature` in HA
- Confirm sensors return numeric values (not "unavailable" or "unknown")
- Monitor serial output for fetch errors
- Check poll interval isn't too long

### Heating Detection Not Working

- Ensure **Heating In Temperature** sensor is configured
- Sensor must update at least every minute
- Temperature must increase by >0.5°C per minute to trigger detection
- Check serial monitor for "Heating ACTIVE detected" messages

### OTA Update Issues

- Ensure device is on same network as computer
- Verify device IP address is correct
- Password is `water-status` (case-sensitive)
- Check firewall isn't blocking port 3232
- Serial monitor shows "OTA Ready" and hostname on startup
- LED turns blue during update, green on success, red on error

### Display Test Mode Stuck

- Click **Test Display** button again to toggle off
- Device immediately polls HA and returns to production mode
- Check serial output confirms "Test mode stopped"

## Development

### Project Structure

```
water-status-esp32/
├── platformio.ini          # PlatformIO configuration
├── include/
│   ├── config.h           # Configuration management
│   ├── display.h          # Display driver interface
│   └── web_interface.h    # HTML web page
├── src/
│   ├── main.cpp           # Main application
│   ├── config.cpp         # Configuration implementation
│   └── display.cpp        # Display implementation
└── README.md
```

### Libraries Used

- **LovyanGFX**: Hardware-accelerated display driver for ST7789
- **ArduinoJson**: JSON parsing (minimal usage to avoid memory issues)
- **HTTPClient**: Home Assistant REST API communication with connection reuse
- **Adafruit NeoPixel**: RGB LED control
- **Preferences**: ESP32 NVS persistent storage
- **WebServer**: Built-in HTTP server for configuration
- **DNSServer**: Captive portal for AP mode

### Performance Optimizations

- HTTP connection reuse (keep-alive) for reduced latency
- Display refresh only when content changes (needsRedraw flag)
- String-based JSON parsing to avoid memory allocation issues
- WiFi auto-reconnection every 30 seconds
- All timing values defined as constants for easy tuning

## Customization

### Changing Display Timing

Edit constants in `src/main.cpp`:

```cpp
const unsigned long DISPLAY_UPDATE_INTERVAL = 1000;        // Display refresh rate
const unsigned long BATH_IMAGE_DISPLAY_TIME = 4000;        // Bath image duration
const unsigned long ROOM_TEMP_DISPLAY_TIME = 2000;         // Room temp duration
const unsigned long LED_FLASH_INTERVAL_NOT_READY = 500;    // LED flash speed
```

### Changing Bath Logic

Modify logic in `src/main.cpp` pollHomeAssistant() function:

```cpp
bathIsReady = (outPipeTemp >= config.min_out_pipe_temp) || 
              (outPipeTemp < tankTemp && tankTemp >= config.min_tank_temp);
```

### Changing Heating Detection Sensitivity

Edit constants in `src/main.cpp`:

```cpp
const float HEATING_TEMP_THRESHOLD = 0.5;  // °C per minute to detect heating
const float HEATING_TEMP_DECREASE = 1.0;   // °C per minute to detect stop
```

### Adding More Sensors

1. Add entity ID field to `include/config.h` Config struct
2. Add NVS storage in `src/config.cpp` load/save methods
3. Add web form field in `src/main.cpp` handleConfig()
4. Add fetch call in `src/main.cpp` pollHomeAssistant()
5. Update display in `src/display.cpp` as needed

## Libraries Used

- **PubSubClient**: MQTT client
- **ESPAsyncWebServer**: Async web server
- **TFT_eSPI**: Display driver for ST7789
- **ArduinoJson**: JSON parsing
- **Preferences**: Non-volatile storage

## Customization

### Changing Display Layout

Edit [src/display.cpp](src/display.cpp) - modify the `drawTemperatures()` and `drawStatus()` functions.

### Adding More Sensors

1. Add topics to [include/config.h](include/config.h)
2. Update web interface in [include/web_interface.h](include/web_interface.h)
3. Add MQTT subscription in [src/main.cpp](src/main.cpp)
4. Update display layout in [src/display.cpp](src/display.cpp)

### Custom Bath Logic

Modify the `updateTemperature()` method in [src/display.cpp](src/display.cpp) to implement custom bath readiness logic.

## License

This project is open source and available for personal and commercial use.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.

## Version History

- **v1.0.0** - Initial MQTT-based version
- **v1.1.0** - Switched to Home Assistant REST API
- **v1.2.0** - Added heating detection and visual indicators
- **v1.3.0** - Performance optimizations and security improvements:
  - HTTP connection reuse
  - Display refresh optimization
  - WiFi auto-reconnection
  - POST for sensitive data
  - Input validation
  - Comprehensive documentation

## Credits

- Built for Waveshare ESP32-C6 1.47" Display Development Board
- Uses LovyanGFX library for hardware-accelerated graphics
- Home Assistant integration via REST API
- Baby bath image display for ready notification

---

**Enjoy your perfectly timed baths! 🛁💧**
