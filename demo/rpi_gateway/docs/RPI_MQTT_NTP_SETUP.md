# Raspberry Pi Workshop Setup (Simple Router Mode)

This version is simplified for class use.

Network design:

- 4G router provides Wi-Fi internet
- Raspberry Pi connects to the same router Wi-Fi
- ESP32 devices connect to the same router Wi-Fi
- Raspberry Pi hosts MQTT broker
- ESP32 uses public NTP server (internet)
- Raspberry Pi does **not** create a Wi-Fi AP

## 1) Fast Setup (Recommended)

Run interactive setup script:

```bash
cd ~/rpi_gateway
sudo bash scripts/setup_rpi_workshop.sh
```

The script will ask for:

- MQTT username
- MQTT password
- Public NTP server for ESP32 (example `pool.ntp.org`)
- Enable UFW or not

Then run validation:

```bash
sudo bash scripts/validate_rpi_workshop.sh
```

## 2) What the Script Configures

### 2.1 Mosquitto (MQTT)

- Installs `mosquitto` and `mosquitto-clients`
- Creates `/etc/mosquitto/passwd`
- Writes `/etc/mosquitto/conf.d/local-auth.conf`:

```conf
listener 1883 0.0.0.0
allow_anonymous false
password_file /etc/mosquitto/passwd
```

### 2.2 Chrony (NTP)

- Installs `chrony`
- Keeps default internet NTP synchronization for Raspberry Pi clock
- Enables and restarts `chrony`

### 2.3 Firewall (optional)

If enabled, it installs/configures UFW rules:

- `22/tcp` (SSH)
- `1883/tcp` (MQTT)

## 3) ESP32 Values to Use

After setup, find Raspberry Pi IP:

```bash
hostname -I
```

Use the first IP in ESP32 firmware/app:

- MQTT broker host: `<rpi-ip>`
- MQTT port: `1883`
- MQTT username/password: from setup script
- NTP server: public NTP such as `pool.ntp.org` or `time.google.com`

## 4) Quick Troubleshooting

- Mosquitto not running:
  - `sudo systemctl status mosquitto --no-pager`
  - `sudo journalctl -xeu mosquitto.service --no-pager`

- Chrony not running:
  - `sudo systemctl status chrony --no-pager`
  - `sudo journalctl -xeu chrony.service --no-pager`

- MQTT login fails:
  - reset password: `sudo mosquitto_passwd /etc/mosquitto/passwd <username>`
  - restart: `sudo systemctl restart mosquitto`

- ESP32 cannot reach RPi:
  - make sure both are on same router Wi-Fi
  - check RPi IP again with `hostname -I`
  - test locally: `mosquitto_pub` + `mosquitto_sub`
