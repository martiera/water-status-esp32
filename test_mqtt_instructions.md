# Installation instructions for MQTT test script

## Install Python dependencies

```bash
pip install paho-mqtt
```

## Usage

Edit the script and update these variables:
- MQTT_BROKER: Your MQTT broker IP address
- MQTT_PORT: Usually 1883
- MQTT_USER/MQTT_PASSWORD: If authentication is required
- TOPICS: Match these with your device configuration

Then run:

```bash
python test_mqtt.py
```
