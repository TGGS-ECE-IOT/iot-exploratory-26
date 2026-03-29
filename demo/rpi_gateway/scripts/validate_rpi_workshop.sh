#!/usr/bin/env bash
set -euo pipefail

# Ultra-simple validation for router Wi-Fi workshop mode.
# Checks:
# - mosquitto/chrony services
# - MQTT (1883/tcp) port
# - local MQTT pub/sub login test

if [[ "$(id -u)" -ne 0 ]]; then
  echo "Please run as root: sudo bash $0"
  exit 1
fi

pass() { echo "[PASS] $1"; }
fail() { echo "[FAIL] $1"; }

check_service() {
  local svc="$1"
  if systemctl is-active --quiet "$svc"; then
    pass "service $svc is active"
  else
    fail "service $svc is NOT active"
    systemctl status "$svc" --no-pager || true
  fi
}

echo "=== Service checks ==="
check_service mosquitto
check_service chrony

echo
echo "=== Port checks ==="
if ss -ltn | grep -q ':1883 '; then
  pass "MQTT port 1883/tcp is listening"
else
  fail "MQTT port 1883/tcp is not listening"
fi

echo
echo "=== MQTT local pub/sub test ==="
read -r -p "MQTT username [actuator]: " MQTT_USER
MQTT_USER="${MQTT_USER:-actuator}"
read -r -s -p "MQTT password for '${MQTT_USER}': " MQTT_PASS
echo

if [[ -z "$MQTT_PASS" ]]; then
  fail "MQTT password is empty, skipping pub/sub test"
  exit 1
fi

TMP_OUT="$(mktemp)"
cleanup() { rm -f "$TMP_OUT" /tmp/workshop_sub.err /tmp/workshop_pub.err; }
trap cleanup EXIT

set +e
mosquitto_sub -h 127.0.0.1 -u "$MQTT_USER" -P "$MQTT_PASS" -t workshop/health -C 1 -W 5 >"$TMP_OUT" 2>/tmp/workshop_sub.err &
SUB_PID=$!
sleep 1
mosquitto_pub -h 127.0.0.1 -u "$MQTT_USER" -P "$MQTT_PASS" -t workshop/health -m "ok" 2>/tmp/workshop_pub.err
PUB_RC=$?
wait "$SUB_PID"
SUB_RC=$?
set -e

if [[ $PUB_RC -eq 0 && $SUB_RC -eq 0 ]] && grep -q '^ok$' "$TMP_OUT"; then
  pass "MQTT pub/sub login test succeeded"
else
  fail "MQTT pub/sub login test failed"
  echo "Subscriber error (if any):"
  cat /tmp/workshop_sub.err || true
  echo "Publisher error (if any):"
  cat /tmp/workshop_pub.err || true
fi

echo
echo "=== Extra checks ==="
chronyc tracking || true
echo
echo "Validation complete."
