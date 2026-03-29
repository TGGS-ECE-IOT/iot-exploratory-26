# ESP32 Sensor Hub Demo

## IoT Exploratory 2026

Organized by Electrical and Computer Engineering Program (ECE), Thai-German Graduate School of Engineering (TGGS), King Mongkut's University of Technology North Bangkok (KMUTNB)

## Overview

ESP-IDF firmware (via PlatformIO) for an ESP32-based sensor node used in the IoT Exploratory 2026 demo.

The node reads multiple sensors, shows status/data on a 128x64 OLED (yellow/blue split), stores network config in NVS, connects to Wi-Fi + MQTT through an RPi gateway, and publishes telemetry for cloud forwarding.

## Highlights

- Modular ESP-IDF component layout (`components/sensor_hub`)
- Web-based first-time setup over config AP (`TGGS_SENSOR_NODE`)
- NVS-persisted Wi-Fi/MQTT/NTP settings
- MQTT telemetry publishing and command subscription
- U8g2 OLED UI with splash + runtime status/sensor views
- Passive buzzer support (short POST tone by default)

## Project Layout

- `src/main.c`: minimal entrypoint, calls `sensor_hub_start()`
- `components/sensor_hub/`: app modules (Wi-Fi, MQTT, UI, sensors, buzzer, state)
- `components/u8g2/`: U8g2 ESP-IDF component integration
- `assets/`: image assets used for splash/logo conversion
- `MQTT_DATA_STRUCTURE.md`: MQTT packet/topic design reference

## Hardware Pins

Defined in `components/sensor_hub/include/pin_config.h`.

- Buzzer: GPIO23
- LEDs: Red GPIO25, Yellow GPIO26, Green GPIO27
- OLED I2C: SDA GPIO21, SCL GPIO22
- DHT22: GPIO13
- HC-SR04: TRIG GPIO32, ECHO GPIO33
- PIR: GPIO35
- ADC channels:
  - MQ135: ADC1 CH6 (GPIO34)
  - LDR: ADC1 CH3 (GPIO39)
  - POT: ADC1 CH0 (GPIO36)

## Build Environment

- Framework: ESP-IDF
- PlatformIO env: `esp32-sensor`
- Board: `esp32dev`
- Flash size: 2MB
- Partition table: `partitions.csv`

From project root:

```bash
pio run -e esp32-sensor
```

Flash and monitor:

```bash
pio run -e esp32-sensor -t upload
pio device monitor -b 115200
```

## First-Time Setup Flow

1. Boot device.
2. If no saved config (or Wi-Fi fails), device starts AP mode:
   - SSID: `TGGS_SENSOR_NODE`
   - Password: `tggs_kmutnb`
3. Connect to AP and open `http://sensor.tggs/`.
   - Fallback: `http://192.168.4.1/`
4. Submit:
   - Wi-Fi SSID/password
   - MQTT host/port/credentials
   - NTP host
   - topic base, site/node metadata, publish interval
5. Device saves config to NVS and reconnects in STA mode.

Status endpoint while in AP mode:

- `http://sensor.tggs/status`
- Fallback: `http://192.168.4.1/status`

## Runtime Behavior

- On boot: OLED splash + short POST buzzer tone.
- If Wi-Fi and MQTT are connected: blue area shows sensor data.
- If not connected to gateway: blue area shows AP credentials and setup URL.
- Green LED indicates Wi-Fi link; other LEDs also reflect sensor conditions in current firmware.

## MQTT (Implemented)

Implemented topic format in code:

- Uplink:
  - `{topic_base}/{site_id}/{node_id}/up/telemetry/periodic`
  - `{topic_base}/{site_id}/{node_id}/up/event/pir`
  - `{topic_base}/{site_id}/{node_id}/up/event/pot`
  - `{topic_base}/{site_id}/{node_id}/up/status/playback`
  - `{topic_base}/{site_id}/{node_id}/up/ack`
- Downlink subscriptions:
  - `{topic_base}/{site_id}/{node_id}/down/cmd/playback`
  - `{topic_base}/{site_id}/{node_id}/down/cmd/display`
  - `{topic_base}/{site_id}/{node_id}/down/cmd/led`

Implemented command handling:

- `cmd/playback` with `action` (`play`/`stop`) and `song_id`
  (`happy_birthday`, `jingle_bells`, `loy_krathong`)
- `cmd/display` and `cmd/led` currently return ACK `not implemented`
- Legacy reboot command payload containing `{"op":"reboot"}` is still supported

Detailed packet structures are documented in `MQTT_DATA_STRUCTURE.md`.

## Notes

- This repo uses PlatformIO build output under `.pio/` (ignored by `.gitignore`).
- If you remove dependencies/build artifacts, regenerate with PlatformIO commands above.
