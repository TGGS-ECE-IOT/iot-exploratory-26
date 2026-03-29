#!/usr/bin/env bash
set -euo pipefail

# Send simulated actuator commands to MQTT topic:
#   iot/actuator/{deviceId}/cmd

BROKER_HOST="127.0.0.1"
BROKER_PORT="1883"
MQTT_USER="actuator"
MQTT_PASS=""
DEVICE_ID="esp32-act-01"

usage() {
  cat <<'EOF'
Usage:
  bash scripts/test_actuator_mqtt.sh [options]

Options:
  --host <host>          MQTT broker host (default: 127.0.0.1)
  --port <port>          MQTT broker port (default: 1883)
  --user <user>          MQTT username (default: actuator)
  --pass <pass>          MQTT password (if omitted, prompt)
  --device <id>          Actuator device ID (default: esp32-act-01)
  --demo                 Send demo command sequence
  -h, --help             Show help

Example:
  bash scripts/test_actuator_mqtt.sh --host 192.168.0.128 --user actuator --device esp32-act-01 --demo
EOF
}

DEMO_MODE="no"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --host) BROKER_HOST="${2:-}"; shift 2 ;;
    --port) BROKER_PORT="${2:-}"; shift 2 ;;
    --user) MQTT_USER="${2:-}"; shift 2 ;;
    --pass) MQTT_PASS="${2:-}"; shift 2 ;;
    --device) DEVICE_ID="${2:-}"; shift 2 ;;
    --demo) DEMO_MODE="yes"; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
done

if ! command -v mosquitto_pub >/dev/null 2>&1; then
  echo "mosquitto_pub not found. Install with: sudo apt-get install -y mosquitto-clients"
  exit 1
fi

if [[ -z "$MQTT_PASS" ]]; then
  read -r -s -p "MQTT password for '$MQTT_USER': " MQTT_PASS
  echo
fi

TOPIC="iot/actuator/${DEVICE_ID}/cmd"

msg_id() {
  printf "cmd-%(%s)T-%04d" -1 "$RANDOM"
}

ts_utc() {
  date -u +"%Y-%m-%dT%H:%M:%SZ"
}

publish_json() {
  local payload="$1"
  echo "-> publish ${TOPIC}: ${payload}"
  mosquitto_pub -h "$BROKER_HOST" -p "$BROKER_PORT" -u "$MQTT_USER" -P "$MQTT_PASS" -t "$TOPIC" -q 1 -m "$payload"
}

send_fan_on() {
  publish_json "{\"v\":1,\"msg_id\":\"$(msg_id)\",\"ts\":\"$(ts_utc)\",\"target\":\"fan\",\"action\":\"set\",\"params\":{\"on\":true}}"
}

send_servo_90() {
  publish_json "{\"v\":1,\"msg_id\":\"$(msg_id)\",\"ts\":\"$(ts_utc)\",\"target\":\"servo\",\"action\":\"set\",\"params\":{\"position_deg\":90}}"
}

send_led_green() {
  publish_json "{\"v\":1,\"msg_id\":\"$(msg_id)\",\"ts\":\"$(ts_utc)\",\"target\":\"led\",\"action\":\"set\",\"params\":{\"color\":\"green\"}}"
}

send_buzzer_beep() {
  publish_json "{\"v\":1,\"msg_id\":\"$(msg_id)\",\"ts\":\"$(ts_utc)\",\"target\":\"buzzer\",\"action\":\"beep\",\"params\":{\"count\":2,\"on_ms\":120,\"off_ms\":120}}"
}

send_oled_message() {
  publish_json "{\"v\":1,\"msg_id\":\"$(msg_id)\",\"ts\":\"$(ts_utc)\",\"target\":\"oled\",\"action\":\"show\",\"params\":{\"line1\":\"RPi Test\",\"line2\":\"MQTT Command\",\"line3\":\"Actuator OK\",\"line4\":\"$(date +%H:%M:%S)\"}}"
}

send_get_status() {
  publish_json "{\"v\":1,\"msg_id\":\"$(msg_id)\",\"ts\":\"$(ts_utc)\",\"target\":\"system\",\"action\":\"get_status\",\"params\":{}}"
}

if [[ "$DEMO_MODE" == "yes" ]]; then
  echo "Sending demo command sequence to device '${DEVICE_ID}'"
  send_get_status
  sleep 1
  send_fan_on
  sleep 1
  send_servo_90
  sleep 1
  send_led_green
  sleep 1
  send_buzzer_beep
  sleep 1
  send_oled_message
  sleep 1
  send_get_status
  echo "Done. Watch ack/status topics with monitor script."
  exit 0
fi

echo "Choose command:"
echo "  1) fan on"
echo "  2) servo 90"
echo "  3) led green"
echo "  4) buzzer beep"
echo "  5) oled message"
echo "  6) get status"
read -r -p "Enter number [1-6]: " PICK

case "$PICK" in
  1) send_fan_on ;;
  2) send_servo_90 ;;
  3) send_led_green ;;
  4) send_buzzer_beep ;;
  5) send_oled_message ;;
  6) send_get_status ;;
  *) echo "Invalid selection"; exit 1 ;;
esac
