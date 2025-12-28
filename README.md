# Water Status ESP32-C6 Monitor

ESP32-C6 water temperature monitoring system with visual display and Home Assistant integration.

## Features

- 🌡️ **4 Temperature Sensors**: Tank, out pipe, heating inlet, room
- 🛁 **Smart Bath Detection**: Visual notification when water is ready
- 🔥 **Heating Indicator**: Animated display when heating system is active
- 🚦 **RGB LED Status**: Red (not ready), Orange (heating), Green (ready), Blue (OTA)
- 📱 **Web Interface**: Easy configuration at `http://<device-ip>/`
- 🏠 **Home Assistant Integration**: REST API sensor polling
- 📡 **OTA Updates**: Flash firmware wirelessly

## Quick Start

### 1. Flash Firmware

```bash
git clone <repository-url>
cd water-status-esp32
pio run --target upload
```

### 2. Connect to WiFi

- Device creates AP: **Water-Status-AP**
- Connect and select your WiFi network
- Device restarts and shows IP address

### 3. Configure

Open `http://<device-ip>/` in browser:

1. **Home Assistant**: Enter URL and long-lived access token, click "Test Connection"
2. **Sensors**: Click "Load Sensors" and select your temperature sensors
3. **Thresholds**: Min Tank (52°C), Min Out Pipe (38°C), Poll Interval (10s)
4. **Display**: Brightness 0-255 (default: 80)

Click **Save** - changes apply immediately, no reboot needed.

## Bath Logic

Bath is ready when: `(Out Pipe ≥ 38°C) OR (Tank ≥ 52°C AND Out Pipe < Tank)`

## Display Modes

- **Not Ready**: Large red STOP sign
- **Ready**: Alternates between baby bath image (4s) and room temperature (2s)
- **Heating Active**: Animated vertical bars on sides

## OTA Updates

After initial USB flash, update wirelessly:

```bash
pio run --target upload --upload-port 192.168.1.XXX
```

Or configure in `platformio.ini`:
```ini
upload_protocol = espota
upload_port = 192.168.1.XXX
upload_flags = --auth=water-status
```

**Password**: `water-status`

## API Endpoints

- `GET /` - Configuration interface
- `GET /status` - JSON sensor data
- `GET /display-test` - Toggle test mode

## Troubleshooting

- **WiFi Issues**: Look for "Water-Status-AP" AP
- **No HA Connection**: Verify URL format and token
- **No Updates**: Check entity IDs have `device_class: temperature`
- **OTA Failed**: Uncomment `upload_flags` in platformio.ini

## Hardware

Waveshare ESP32-C6 1.47" Display (320×172, ST7789) with built-in RGB LED.

---

**Enjoy perfectly timed baths! 🛁**
