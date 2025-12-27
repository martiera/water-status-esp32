# Example MQTT test script for Water Status Monitor
# This script publishes random temperature values to test your device

import paho.mqtt.client as mqtt
import random
import time
import sys

# Configuration
MQTT_BROKER = "192.168.1.100"  # Change to your broker IP
MQTT_PORT = 1883
MQTT_USER = ""  # Optional
MQTT_PASSWORD = ""  # Optional

# Topics (should match your device configuration)
TOPICS = {
    "tank": "home/water/tank/temperature",
    "outpipe": "home/water/outpipe/temperature",
    "heating_in": "home/water/heating/in/temperature",
    "heating_out": "home/water/heating/out/temperature"
}

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("✓ Connected to MQTT broker")
    else:
        print(f"✗ Failed to connect, return code {rc}")
        sys.exit(1)

def generate_realistic_temps():
    """Generate realistic water system temperatures"""
    return {
        "tank": random.uniform(38, 55),        # Tank: 38-55°C
        "outpipe": random.uniform(35, 50),     # Out pipe: slightly cooler
        "heating_in": random.uniform(50, 70),  # Heating in: hotter
        "heating_out": random.uniform(30, 50)  # Heating out: cooler
    }

def main():
    print("Water Status Monitor - MQTT Test Publisher")
    print("=" * 50)
    print(f"Broker: {MQTT_BROKER}:{MQTT_PORT}")
    print("Topics:")
    for name, topic in TOPICS.items():
        print(f"  - {name}: {topic}")
    print("=" * 50)
    
    # Create MQTT client
    client = mqtt.Client()
    client.on_connect = on_connect
    
    if MQTT_USER:
        client.username_pw_set(MQTT_USER, MQTT_PASSWORD)
    
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        client.loop_start()
        
        print("\nPublishing temperature data every 5 seconds...")
        print("Press Ctrl+C to stop\n")
        
        while True:
            temps = generate_realistic_temps()
            
            for name, topic in TOPICS.items():
                temp = temps[name]
                client.publish(topic, f"{temp:.1f}")
                print(f"📤 {name:12s}: {temp:5.1f}°C → {topic}")
            
            print("-" * 50)
            time.sleep(5)
            
    except KeyboardInterrupt:
        print("\n\n✓ Stopped by user")
    except Exception as e:
        print(f"\n✗ Error: {e}")
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
