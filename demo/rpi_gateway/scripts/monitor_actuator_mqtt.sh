#!/usr/bin/env bash
set -euo pipefail

# Monitor actuator ACK/STATUS topics.

BROKER_HOST="127.0.0.1"
BROKER_PORT="1883"
MQTT_USER="actuator"
MQTT_PASS=""
DEVICE_ID="esp32-act-01"

usage() {
  cat <<'EOF'
Usage:
  bash scripts/monitor_actuator_mqtt.sh [options]

Options:
  --host <host>      MQTT broker host (default: 127.0.0.1)
  --port <port>      MQTT broker port (default: 1883)
  --user <user>      MQTT username (default: actuator)
  --pass <pass>      MQTT password (if omitted, prompt)
  --device <id>      Actuator device ID (default: esp32-act-01)
  -h, --help         Show help
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) BROKER_HOST="${2:-}"; shift 2 ;;
    --port) BROKER_PORT="${2:-}"; shift 2 ;;
    --user) MQTT_USER="${2:-}"; shift 2 ;;
    --pass) MQTT_PASS="${2:-}"; shift 2 ;;
    --device) DEVICE_ID="${2:-}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
done

if ! command -v mosquitto_sub >/dev/null 2>&1; then
  echo "mosquitto_sub not found. Install with: sudo apt-get install -y mosquitto-clients"
  exit 1
fi

if [[ -z "$MQTT_PASS" ]]; then
  read -r -s -p "MQTT password for '$MQTT_USER': " MQTT_PASS
  echo
fi

echo "Monitoring actuator topics for device '${DEVICE_ID}'"
echo "Press Ctrl+C to stop"

mosquitto_sub \
  -h "$BROKER_HOST" \
  -p "$BROKER_PORT" \
  -u "$MQTT_USER" \
  -P "$MQTT_PASS" \
  -v \
  -q 1 \
  -t "iot/actuator/${DEVICE_ID}/ack" \
  -t "iot/actuator/${DEVICE_ID}/status" \
  -t "iot/actuator/${DEVICE_ID}/availability"
