# ESP32 Actuator Demo

## IoT Exploratory 2026

Organized by Electrical and Computer Engineering Program (ECE), Thai-German Graduate School of Engineering (TGGS), King Mongkut's University of Technology North Bangkok (KMUTNB)

## Overview

ESP32 (ESP-IDF via PlatformIO) actuator demo with:

- OLED SSD1306 128x32 over I2C
- Relay-controlled fan
- Servo position control
- 3 LEDs (traffic style)
- Active buzzer
- Push button
- MQTT-based cloud command + status reporting
- WiFi AP configuration portal fallback

## Hardware

Main board: `esp32dev`

Current pin mapping is centralized in:

- `components/board_config/include/pin_config.h`

Default mapping:

- OLED SDA: GPIO21
- OLED SCL: GPIO22
- Servo: GPIO18
- Relay: GPIO13
- Buzzer: GPIO23
- LED Red: GPIO25
- LED Yellow: GPIO26
- LED Green: GPIO27
- Button: GPIO32

Relay notes:

- Configured as active-low with OFF = high impedance (high-Z)
- This matches relay modules that can still trigger from 3.3V HIGH when powered from 5V

## Project Structure

- `src/main.c` - application flow (boot, splash, network, MQTT, status)
- `components/oled` - SSD1306 driver + bitmap support
- `components/relay` - relay control logic
- `components/servo` - servo driver (new MCPWM API)
- `components/led` - LED driver
- `components/buzzer` - buzzer driver
- `components/button` - debounced button driver
- `components/post` - power-on self-test
- `components/board_config` - shared pin/config macros
- `assets/ece_logo_128x32.png` - source logo image

## MQTT Contract

MQTT topic and payload design is documented in:

- `MQTT_DESIGN.md`

Includes:

- Topic namespace
- Command payload schema
- Ack/status payload schema
- QoS/retain policy
- Validation rules and OLED behavior

## Runtime Behavior

1. Show splash screens:
   - ECE logo for 1 second
   - TGGS/KMUTNB screen for 1 second
2. Run POST
3. Try stored WiFi + MQTT config from NVS
4. If connected:
   - subscribe MQTT command topic
   - apply actuator commands
   - publish status snapshots
5. If not connected:
    - start AP mode
    - SSID: `IOT_ACTUATOR_DEMO`
    - Password: `tggs_kmutnb`
    - Config URL: `http://actuator.tggs`
    - Fallback IP: `http://192.168.4.1`
    - Wildcard DNS in AP mode resolves any domain to `192.168.4.1`
    - host config webpage for WiFi/MQTT/NTP settings

OLED status display:

- Left side: WiFi / MQTT / Time state
- Right side: actuator state
- Temporary command text shown when MQTT command is received

## Build and Flash

From project root:

```bash
pio run -t clean
pio run
pio run -t upload
```

Serial monitor:

```bash
pio device monitor
```

## Configuration Storage

Device settings are stored in NVS, including:

- WiFi SSID/password
- MQTT URI/user/password/topic
- NTP server

## Notes

- This project uses ESP-IDF components with CMake component dependencies.
- If IntelliSense shows missing ESP-IDF headers, verify build from CLI (`pio run`) before changing code.
