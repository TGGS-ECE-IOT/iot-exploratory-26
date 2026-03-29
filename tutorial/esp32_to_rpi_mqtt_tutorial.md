# ESP32 to Raspberry Pi MQTT Tutorial

## Overview
This tutorial explains how to send sensor data from an ESP32 to a Raspberry Pi using MQTT.

---

## Architecture

Sensor → ESP32 → WiFi → MQTT (Raspberry Pi)

---

## Step 1: Setup Raspberry Pi (MQTT Broker)

### Install Mosquitto
```bash
sudo apt update
sudo apt install -y mosquitto mosquitto-clients python3-pip
```

### Start service
```bash
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

### Check IP address
```bash
hostname -I
```

---

## Step 2: Test MQTT Broker

Terminal 1:
```bash
mosquitto_sub -h localhost -t "iot/test"
```

Terminal 2:
```bash
mosquitto_pub -h localhost -t "iot/test" -m "hello"
```

---

## Step 3: ESP32 Setup (PlatformIO)

### platformio.ini
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = espidf
monitor_speed = 115200
```

---

## Step 4: ESP32 Code (main.c)

```c
// simplified example
printf("Publish sensor data via MQTT\n");
```

(Replace with full code from tutorial if needed)

---

## Step 5: Upload Code
```bash
pio run
pio run --target upload
pio device monitor
```

---

## Step 6: Receive Data on Raspberry Pi

```bash
mosquitto_sub -h <PI_IP> -t "iot/team1/sensor/weather"
```

---

## Step 7: Python Subscriber (Optional)

Install:
```bash
pip3 install paho-mqtt
```

Example:
```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    print(msg.payload.decode())

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.1.50", 1883)
client.subscribe("iot/team1/sensor/weather")
client.loop_forever()
```

---

## Notes
- Ensure ESP32 and Raspberry Pi are on same WiFi
- Use correct IP address
- Check firewall if connection fails

---

## Next Steps
- Add real sensors (DHT22, LDR, etc.)
- Store data in database
- Build dashboard
