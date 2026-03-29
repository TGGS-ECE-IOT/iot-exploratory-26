# RPi Gateway (ESP32 Sensor + Actuator + Pub/Sub)

Python gateway service for Raspberry Pi that:

- Subscribes to local MQTT topics from ESP32 sensor and actuator nodes
- Normalizes messages into a canonical event model
- Forwards data to Google Cloud Pub/Sub
- Accepts cloud commands from Pub/Sub subscription and routes them back to MQTT device topics
- Logs all inbound/outbound messages to a message log file
- Supports offline demo mode with local simulation and store-and-forward spool
- Exposes local observability endpoints (`/health`, `/metrics`)

## Topic Contracts

Implemented based on:

- `../esp32_sensor_demo/MQTT_SENSOR_DATA_STRUCTURE.md`
- `../esp32_actuator_demo/MQTT_ACTUATOR_DESIGN.md`

## Quick Start

For Raspberry Pi MQTT + time sync setup (simple router Wi-Fi mode), see `docs/RPI_MQTT_NTP_SETUP.md`.

For student-friendly one-shot setup, use `scripts/setup_rpi_workshop.sh`.

For quick post-setup checks, run `scripts/validate_rpi_workshop.sh`.

## 1) Install

```bash
python -m venv .venv
. .venv/Scripts/activate
pip install -e .
```

## 2) Configure

Edit `config/gateway.toml`.

For cloud mode, set:

- `cloud_pubsub.enabled = true`
- `cloud_pubsub.project_id`
- `cloud_pubsub.publish_topic`
- `cloud_pubsub.command_subscription`

Also set Google credentials on RPi:

```bash
set GOOGLE_APPLICATION_CREDENTIALS=D:\path\to\service-account.json
```

Linux/RPi shell:

```bash
export GOOGLE_APPLICATION_CREDENTIALS=/opt/gateway/sa.json
```

## 3) Run gateway

```bash
rpi-gateway --config config/gateway.toml run
```

## Commands

- Run normal gateway:
  - `rpi-gateway --config config/gateway.toml run`
- Run offline demo (local MQTT + simulator + spool):
  - `rpi-gateway --config config/gateway.toml offline-demo --interval 2`
- Publish simulated data to Pub/Sub:
  - `rpi-gateway --config config/gateway.toml pubsub-sim --count 20 --interval 0.5`

## Logs

- App log: `logs/gateway.log`
- All MQTT command/data message log: `logs/messages.log`

`messages.log` stores JSON lines with direction:

- `mqtt_in`: device -> gateway
- `mqtt_out`: gateway -> device command forwarding

## Local Observability

- Health: `http://<rpi-ip>:8080/health`
- Metrics: `http://<rpi-ip>:8080/metrics`

Example metrics fields:

- `mqtt_ingress_total`
- `pubsub_publish_ok_total`
- `pubsub_publish_fail_total`
- `spooled_total`
- `spool_flush_ok_total`
- `cloud_command_total`

## Offline Demo Behavior

When running `offline-demo`:

- Cloud forwarding is disabled
- Local simulator publishes periodic sensor + actuator status packets to MQTT
- Gateway parses and writes all events to local spool (`data/spool.db`) and logs
- You can still publish MQTT downlink commands and see simulator print them

## NTP on Raspberry Pi (chrony)

Install and configure chrony so ESP32 and gateway share NTP-synced UTC timestamps.

1. Install:

```bash
sudo apt-get update
sudo apt-get install -y chrony
```

2. Edit `/etc/chrony/chrony.conf`:

```conf
pool pool.ntp.org iburst
allow 192.168.1.0/24
local stratum 10
makestep 1.0 3
```

3. Restart:

```bash
sudo systemctl restart chrony
sudo systemctl enable chrony
chronyc sources -v
```

Then point ESP32 NTP settings to the RPi IP.
