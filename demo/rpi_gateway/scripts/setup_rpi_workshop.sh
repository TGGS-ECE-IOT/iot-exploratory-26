#!/usr/bin/env bash
set -euo pipefail

# Simple workshop setup (no Raspberry Pi Wi-Fi AP mode).
# Topology: RPi + ESP32 devices all connect to the same 4G router Wi-Fi.
# This script configures only:
# - Mosquitto (MQTT broker)
# - Chrony (RPi clock sync)
# - UFW firewall (optional)

MQTT_USER="actuator"
MQTT_PASS=""
PUBLIC_NTP="pool.ntp.org"
ENABLE_UFW="yes"

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  cat <<'EOF'
Run interactive setup:
  sudo bash scripts/setup_rpi_workshop.sh

What it sets up:
  - Mosquitto with username/password login
  - Chrony for Raspberry Pi time synchronization
  - UFW rules for SSH and MQTT (optional)
EOF
  exit 0
fi

if [[ "$(id -u)" -ne 0 ]]; then
  echo "Please run as root: sudo bash $0"
  exit 1
fi

ask_default() {
  local prompt="$1"
  local default="$2"
  local value
  read -r -p "$prompt [$default]: " value
  if [[ -z "$value" ]]; then
    echo "$default"
  else
    echo "$value"
  fi
}

echo "=== RPi Workshop Setup (Router Wi-Fi Mode) ==="
echo "RPi and ESP32 will use the same 4G router network."
echo

MQTT_USER="$(ask_default "MQTT username" "$MQTT_USER")"

while true; do
  read -r -s -p "MQTT password for '$MQTT_USER': " p1
  echo
  read -r -s -p "Confirm MQTT password: " p2
  echo
  if [[ -z "$p1" ]]; then
    echo "Password cannot be empty. Try again."
    continue
  fi
  if [[ "$p1" != "$p2" ]]; then
    echo "Passwords do not match. Try again."
    continue
  fi
  MQTT_PASS="$p1"
  break
done

PUBLIC_NTP="$(ask_default "Public NTP server for ESP32 (example: pool.ntp.org)" "$PUBLIC_NTP")"
UFW_ANSWER="$(ask_default "Enable UFW firewall? (yes/no)" "yes")"
case "${UFW_ANSWER,,}" in
  y|yes) ENABLE_UFW="yes" ;;
  n|no) ENABLE_UFW="no" ;;
  *) ENABLE_UFW="yes" ;;
esac

echo
echo "==> Installing packages"
apt-get update
apt-get install -y mosquitto mosquitto-clients chrony
if [[ "$ENABLE_UFW" == "yes" ]]; then
  apt-get install -y ufw
fi

echo "==> Configuring MQTT credentials"
if [[ -f /etc/mosquitto/passwd ]]; then
  mosquitto_passwd -b /etc/mosquitto/passwd "$MQTT_USER" "$MQTT_PASS"
else
  mosquitto_passwd -b -c /etc/mosquitto/passwd "$MQTT_USER" "$MQTT_PASS"
fi
chown root:mosquitto /etc/mosquitto/passwd
chmod 640 /etc/mosquitto/passwd

echo "==> Writing Mosquitto auth config"
cat >/etc/mosquitto/conf.d/local-auth.conf <<'EOF'
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd
EOF

echo "==> Keeping Chrony defaults (RPi syncs from internet NTP)"

echo "==> Enabling and restarting services"
systemctl enable mosquitto chrony
systemctl restart mosquitto
systemctl restart chrony

if [[ "$ENABLE_UFW" == "yes" ]]; then
  echo "==> Configuring UFW rules"
  ufw allow 22/tcp
  ufw allow 1883/tcp
  ufw --force enable
fi

RPI_IP="$(hostname -I | awk '{print $1}')"

echo
echo "Setup complete."
echo "Run validation script next:"
echo "  sudo bash scripts/validate_rpi_workshop.sh"
echo
echo "Use these values in ESP32:"
echo "  MQTT broker: ${RPI_IP}:1883"
echo "  MQTT username: ${MQTT_USER}"
echo "  NTP server: ${PUBLIC_NTP}"
