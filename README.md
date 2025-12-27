# Water Status ESP32-C6 Monitor

IoT device for monitoring hot water system temperatures and displaying bath readiness status using Waveshare ESP32-C6 1.47" Display (172×320).

## Features

- 📊 **4 Temperature Monitoring Points**:
  - Hot water storage tank
  - Hot water out pipe
  - Heating pipe incoming
  - Heating pipe exiting

- 🎯 **Bath Readiness Detection**: Automatically determines if water is ready for bathing based on configurable temperature thresholds

- 📱 **Web Configuration Interface**: Easy setup through browser-based configuration panel

- 📡 **MQTT Integration**: Subscribe to temperature data from your home automation system

- 🖥️ **Real-time Display**: Color TFT display shows all temperatures and system status

- 💾 **Persistent Configuration**: Settings saved to flash memory

## Hardware Requirements

- **Waveshare ESP32-C6 1.47" Display Development Board** (172×320 resolution)
- USB-C cable for programming and power
- MQTT broker (e.g., Mosquitto, Home Assistant)

## Pin Configuration

The display pins are pre-configured in `platformio.ini`:

```
TFT_MOSI = GPIO 7
TFT_SCLK = GPIO 6
TFT_CS   = GPIO 10
TFT_DC   = GPIO 2
TFT_RST  = GPIO 3
TFT_BL   = GPIO 11
```

These match the Waveshare ESP32-C6 1.47" board pinout.

## Software Setup

### 1. Install PlatformIO

- **VS Code**: Install the PlatformIO IDE extension
- **CLI**: Install PlatformIO Core

### 2. Clone and Build

```bash
cd /Users/martins/WS/github.com/martiera/water-status-esp32
pio run
```

### 3. Upload to Device

```bash
pio run --target upload
```

### 4. Monitor Serial Output

```bash
pio device monitor
```

## Initial Configuration

### First Boot - AP Mode

1. On first boot, the device creates a WiFi access point:
   - **SSID**: `Water-Status-AP`
   - **Password**: `12345678`

2. Connect to the AP and navigate to: `http://192.168.4.1`

3. Configure:
   - WiFi credentials
   - MQTT broker details
   - MQTT topics for each temperature sensor
   - Temperature thresholds for bath readiness

4. Click **Save Configuration** - device will restart and connect to your network

### Normal Operation Mode

Once configured, access the web interface at the device's IP address (shown in serial monitor).

## MQTT Configuration

### Default Topics

The device subscribes to these topics (configurable via web interface):

```
home/water/tank/temperature           # Tank temperature
home/water/outpipe/temperature        # Out pipe temperature
home/water/heating/in/temperature     # Heating incoming
home/water/heating/out/temperature    # Heating exiting
```

### Message Format

Temperature values should be published as simple numeric strings:

```bash
mosquitto_pub -h your-broker -t "home/water/tank/temperature" -m "42.5"
```

### Example Home Assistant Configuration

```yaml
mqtt:
  sensor:
    - name: "Water Tank Temperature"
      state_topic: "home/water/tank/temperature"
      unit_of_measurement: "°C"
      device_class: temperature
```

## Display Interface

### Color Coding

- **Cyan**: Tank temperature
- **Orange**: Out pipe temperature
- **Yellow**: Heating incoming temperature
- **Magenta**: Heating exiting temperature

### Status Indicators

- **Top Left Circle**: WiFi status (Green = connected, Red = disconnected)
- **Top Right Circle**: MQTT status (Green = connected, Red = disconnected)
- **Bottom Bar**: 
  - **Green "BATH READY"**: Temperatures meet threshold requirements
  - **Red "NOT READY"**: Temperatures below threshold

## Configuration Parameters

### Temperature Thresholds

Default values (Celsius):
- **Minimum Tank Temperature**: 52°C
- **Minimum Out Pipe Temperature**: 38°C

### Smart Bath Readiness Logic

The system uses intelligent detection to determine bath readiness:

1. **Initial State**: Tank must be hot (≥ 52°C)
2. **Water Flow Detection**: Monitors if tank temperature is dropping (indicating water usage)
3. **Pipe Warmth**: Out pipe must be warm (≥ 38°C)

**Why this matters**: When the tank is first heated, the out pipe may still be cold since no water has flowed. The device waits until it detects the tank temperature dropping (water flowing) AND the pipe warming up before declaring bath ready.

Once bath becomes ready, it stays ready until:
- Tank temperature drops below threshold, OR
- Out pipe cools down significantly (below 36°C)

### Display Settings

- **Brightness**: 0-255 (default: 200)
- **Temperature Unit**: Celsius or Fahrenheit

## Integration Examples

### Node-RED Flow

```json
[{
    "id": "mqtt-temp-pub",
    "type": "mqtt out",
    "topic": "home/water/tank/temperature",
    "qos": "0",
    "broker": "mqtt-broker",
    "name": "Tank Temperature"
}]
```

### Python MQTT Publisher

```python
import paho.mqtt.client as mqtt
import random
import time

client = mqtt.Client()
client.connect("192.168.1.100", 1883, 60)

while True:
    temp = random.uniform(35, 50)
    client.publish("home/water/tank/temperature", str(temp))
    time.sleep(10)
```

## Troubleshooting

### Display Not Working

- Check pin connections match the configuration
- Verify TFT_BL (backlight) pin is correctly set
- Try adjusting brightness in configuration

### WiFi Connection Failed

- Double-check SSID and password
- Ensure 2.4GHz WiFi (ESP32-C6 doesn't support 5GHz)
- Check signal strength

### MQTT Not Connecting

- Verify broker IP address and port
- Check firewall settings on broker
- Test with MQTT client: `mosquitto_sub -h broker-ip -t "#" -v`

### No Temperature Updates

- Confirm topics match your MQTT setup
- Check MQTT broker logs
- Verify payload format (should be simple numeric string)
- Use serial monitor to see incoming messages

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

## Credits

- Built for Waveshare ESP32-C6 1.47" Display Development Board
- Uses TFT_eSPI library by Bodmer
- MQTT support via PubSubClient

---

**Enjoy your perfectly timed baths! 🛁💧**
