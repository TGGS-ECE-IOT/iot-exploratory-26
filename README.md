# IoT Exploratory 2026

<p align="center">
  <img src="https://tggs.kmutnb.ac.th/wp-content/uploads/2026/02/IoT-2026-Banner-2.png" alt="IoT Exploratory 2026 Banner" width="100%">
</p>

This repository contains workshop demos and tutorials used in the IoT Exploratory 2026 program by TGGS, KMUTNB.

Official event details:
- https://tggs.kmutnb.ac.th/iot-exploratory-2026

## Event at a glance

- Format: 5-day immersive IoT and AI workshop
- Venue: TGGS, KMUTNB, Bangkok, Thailand
- Dates: March 30 - April 3, 2026
- Team size: 3 university students per team
- Support grant: up to 15,000 THB per selected team (subject to official conditions)
- Contact: iot-exploratory@tggs.kmutnb.ac.th

## Repository structure

- `demo/`
  - `esp32_sensor_demo/`: ESP32 sensor node firmware (PlatformIO + ESP-IDF)
  - `esp32_actuator_demo/`: ESP32 actuator node firmware (PlatformIO + ESP-IDF)
  - `rpi_gateway/`: Raspberry Pi gateway service (Python + MQTT + optional Google Cloud Pub/Sub)
- `tutorial/`
  - Workshop guides, setup notes, hardware tutorials, and reference materials
- `lecture/`
  - Lecture and reference materials, script examples for *Data Analytics and AI* workshop

## Quick start

1. Review workshop context in `tutorial/00_workshop_overview.md`.
2. Build and test ESP32 demos in:
   - `demo/esp32_sensor_demo`
   - `demo/esp32_actuator_demo`
3. Configure and run the gateway in `demo/rpi_gateway`.
4. Follow topic contracts and setup notes in each demo README.

## Prerequisites

- PlatformIO (for ESP32 firmware projects)
- Python 3.11+ (for `demo/rpi_gateway`)
- MQTT broker (local or network-accessible)
- Raspberry Pi (recommended for gateway deployment)

## Notes

- This repo includes source code, workshop tutorials, and selected supporting assets.
- Build output, local environments, and runtime logs should remain untracked (see `.gitignore`).
