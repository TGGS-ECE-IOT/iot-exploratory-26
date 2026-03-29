# ESP32 → Raspberry Pi via Bluetooth → Google Cloud Pub/Sub

This tutorial shows how to build a simple IoT pipeline:

- **ESP32** reads sensor data
- **ESP32** sends the data to **Raspberry Pi** over **Bluetooth Classic (SPP)**
- **Raspberry Pi** receives the data and forwards it to **Google Cloud Pub/Sub**
- Later, you can connect Pub/Sub to BigQuery, Dataflow, Cloud Functions, or dashboards

This design is good for workshops because:

- the ESP32 does not need direct Internet access
- the Raspberry Pi acts as a local gateway
- Google Cloud Pub/Sub is designed for decoupled event ingestion and supports publisher/subscriber patterns well citeturn638984search3turn638984search12

---

## 1) System architecture

```text
+------------------+        Bluetooth SPP         +---------------------+        HTTPS/API        +----------------------+
| ESP32 sensor node|  --------------------------> | Raspberry Pi gateway|  --------------------> | Google Cloud Pub/Sub |
| - read sensor    |                              | - receive JSON      |                        | - topic              |
| - format JSON    |                              | - validate/parse    |                        | - downstream systems |
| - send via BT    |                              | - publish to cloud  |                        |                      |
+------------------+                              +---------------------+                        +----------------------+
```

### Recommended data flow

1. ESP32 reads sensor values.
2. ESP32 formats the values as one JSON line.
3. ESP32 sends the JSON line over Bluetooth Serial.
4. Raspberry Pi reads one line at a time.
5. Raspberry Pi publishes the same message to a Pub/Sub topic.
6. Google Cloud services consume the topic.

---

## 2) Why use Bluetooth Classic instead of BLE here?

For a workshop, **Bluetooth Classic Serial Port Profile (SPP)** is usually the simplest option because it behaves like a serial stream between ESP32 and Raspberry Pi. That means:

- the ESP32 code is simpler
- the Raspberry Pi side can read text lines like normal serial data
- it is easy to send newline-delimited JSON

BLE is possible, but it adds more setup because you must define services, characteristics, notifications, and the receive logic.

---

## 3) Hardware used

### ESP32 side

- 1 × ESP32 development board
- 1 × sensor
  - example: DHT22, LDR, ultrasonic, PIR, etc.

### Raspberry Pi side

- Raspberry Pi 4 or 5
- Raspberry Pi OS
- built-in Bluetooth or Bluetooth USB dongle
- internet connection for Google Cloud access

---

## 4) Software stack

### ESP32

- **PlatformIO**
- **Arduino framework for ESP32**
- `BluetoothSerial` library

### Raspberry Pi

- Python 3
- `pyserial` or Bluetooth RFCOMM access
- `google-cloud-pubsub` Python package

### Google Cloud

- Google Cloud project
- Pub/Sub API enabled
- Pub/Sub topic created
- service account or local ADC authentication configured

Google recommends the high-level Pub/Sub client library for publishing and receiving messages. Pub/Sub also supports pull subscriptions, with the high-level client and StreamingPull typically recommended for most subscriber use cases. citeturn638984search10turn638984search4

---

## 5) Message format

Use one JSON object per line.

Example:

```json
{"device_id":"esp32-node-01","temperature":28.4,"humidity":74.1,"light":512,"ts":"2026-03-25T10:30:00Z"}
```

Why newline-delimited JSON?

- easy to debug on both sides
- easy to parse on Raspberry Pi
- easy to publish directly to Pub/Sub

---

## 6) Step A — Create the Google Cloud resources

## 6.1 Enable Pub/Sub API

In your Google Cloud project, enable:

- **Pub/Sub API**

## 6.2 Create a topic

Example topic name:

- `iot_sensor_ingest`

You can create it in Cloud Shell:

```bash
gcloud pubsub topics create iot_sensor_ingest
```

## 6.3 Create a service account for the Raspberry Pi gateway

Example service account:

- `rpi-gateway-publisher`

Grant it a role that can publish to Pub/Sub, such as:

- `Pub/Sub Publisher`

Example:

```bash
gcloud iam service-accounts create rpi-gateway-publisher \
  --display-name="Raspberry Pi Gateway Publisher"

gcloud projects add-iam-policy-binding YOUR_PROJECT_ID \
  --member="serviceAccount:rpi-gateway-publisher@YOUR_PROJECT_ID.iam.gserviceaccount.com" \
  --role="roles/pubsub.publisher"
```

## 6.4 Create a key file

```bash
gcloud iam service-accounts keys create ~/rpi-gateway-key.json \
  --iam-account=rpi-gateway-publisher@YOUR_PROJECT_ID.iam.gserviceaccount.com
```

Copy this JSON key securely to the Raspberry Pi.

> For a classroom environment, you can also use a shared project and create one topic per team. Pub/Sub supports publisher and subscriber decoupling, so this pattern scales well for workshops. citeturn638984search3turn638984search14

---

## 7) Step B — Prepare the Raspberry Pi

## 7.1 Install system packages

```bash
sudo apt update
sudo apt install -y python3 python3-pip bluetooth bluez bluez-tools
```

## 7.2 Install Python package

```bash
pip3 install google-cloud-pubsub
```

## 7.3 Copy the service account key

Example:

```bash
mkdir -p ~/gcp-keys
cp rpi-gateway-key.json ~/gcp-keys/
chmod 600 ~/gcp-keys/rpi-gateway-key.json
```

## 7.4 Set the authentication environment variable

```bash
export GOOGLE_APPLICATION_CREDENTIALS=~/gcp-keys/rpi-gateway-key.json
```

To make it persistent, add it to `~/.bashrc`:

```bash
echo 'export GOOGLE_APPLICATION_CREDENTIALS=~/gcp-keys/rpi-gateway-key.json' >> ~/.bashrc
source ~/.bashrc
```

---

## 8) Step C — Pair Raspberry Pi with the ESP32

On the Raspberry Pi:

```bash
bluetoothctl
```

Then run:

```text
power on
agent on
default-agent
scan on
```

Wait until the ESP32 appears. Then:

```text
pair XX:XX:XX:XX:XX:XX
trust XX:XX:XX:XX:XX:XX
connect XX:XX:XX:XX:XX:XX
```

Replace `XX:XX:XX:XX:XX:XX` with the ESP32 Bluetooth MAC address.

In many Raspberry Pi Bluetooth Classic workflows, the next step is to bind or connect an RFCOMM device such as `/dev/rfcomm0`.

Example:

```bash
sudo rfcomm bind /dev/rfcomm0 XX:XX:XX:XX:XX:XX 1
```

Check:

```bash
ls -l /dev/rfcomm0
```

> Depending on Raspberry Pi OS version and BlueZ behavior, you may need to reconnect or rebind after reboot.

---

## 9) Step D — ESP32 code to send sensor data by Bluetooth

This example uses:

- a simulated analog value for `light`
- a generated temperature/humidity value
- Bluetooth device name: `ESP32_Sensor_Node`

Create a PlatformIO project.

### `platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
```

### `src/main.cpp`

```cpp
#include <Arduino.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

static const char *DEVICE_ID = "esp32-node-01";

void setup() {
  Serial.begin(115200);

  if (!SerialBT.begin("ESP32_Sensor_Node")) {
    Serial.println("Bluetooth start failed");
    while (true) {
      delay(1000);
    }
  }

  Serial.println("Bluetooth started. Waiting for Raspberry Pi...");
}

void loop() {
  // Replace these with real sensor readings
  float temperature = 25.0 + (random(0, 100) / 10.0f);
  float humidity = 50.0 + (random(0, 300) / 10.0f);
  int light = analogRead(34);  // example ADC pin

  // Simple ISO-like timestamp placeholder generated on gateway side if needed
  String payload = "{";
  payload += "\"device_id\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"humidity\":" + String(humidity, 1) + ",";
  payload += "\"light\":" + String(light);
  payload += "}";

  Serial.println(payload);
  SerialBT.println(payload);

  delay(5000);
}
```

### Notes

- `SerialBT.println(payload);` sends one complete JSON record per line.
- The Raspberry Pi can receive it as a normal text stream.
- For real deployment, add sensor validity checks and reconnect logic.

---

## 10) Step E — Raspberry Pi Python gateway

This script does three jobs:

1. reads one line from Bluetooth RFCOMM
2. validates the JSON
3. publishes it to Google Cloud Pub/Sub

Create file `rpi_bt_to_pubsub.py`.

```python
import json
import time
from datetime import datetime, timezone

from google.cloud import pubsub_v1

RFCOMM_DEVICE = "/dev/rfcomm0"
PROJECT_ID = "YOUR_PROJECT_ID"
TOPIC_ID = "iot_sensor_ingest"

publisher = pubsub_v1.PublisherClient()
topic_path = publisher.topic_path(PROJECT_ID, TOPIC_ID)


def publish_message(data: dict) -> None:
    payload = json.dumps(data).encode("utf-8")
    future = publisher.publish(topic_path, payload)
    message_id = future.result()
    print(f"Published message ID: {message_id}")


def enrich_payload(data: dict) -> dict:
    if "gateway_ts" not in data:
        data["gateway_ts"] = datetime.now(timezone.utc).isoformat()
    return data


def main() -> None:
    while True:
        try:
            with open(RFCOMM_DEVICE, "r", encoding="utf-8", errors="ignore") as bt:
                print(f"Connected to {RFCOMM_DEVICE}")
                while True:
                    line = bt.readline()
                    if not line:
                        time.sleep(0.1)
                        continue

                    line = line.strip()
                    if not line:
                        continue

                    print(f"RX: {line}")

                    try:
                        data = json.loads(line)
                        data = enrich_payload(data)
                        publish_message(data)
                    except json.JSONDecodeError:
                        print("Invalid JSON, skipped")
                    except Exception as exc:
                        print(f"Publish error: {exc}")
        except FileNotFoundError:
            print(f"{RFCOMM_DEVICE} not found. Retrying in 5 seconds...")
            time.sleep(5)
        except Exception as exc:
            print(f"Gateway error: {exc}")
            time.sleep(5)


if __name__ == "__main__":
    main()
```

Run it:

```bash
python3 rpi_bt_to_pubsub.py
```

---

## 11) Step F — Verify the messages in Google Cloud

Create a subscription for testing:

```bash
gcloud pubsub subscriptions create iot_sensor_ingest-sub --topic=iot_sensor_ingest
```

Pull messages:

```bash
gcloud pubsub subscriptions pull iot_sensor_ingest-sub --auto-ack --limit=5
```

Pub/Sub supports pull subscriptions, and Google documents pull and StreamingPull workflows for subscribers. citeturn638984search16turn638984search4

You should see JSON payloads similar to:

```json
{
  "device_id": "esp32-node-01",
  "temperature": 28.4,
  "humidity": 74.1,
  "light": 512,
  "gateway_ts": "2026-03-25T10:30:00.000000+00:00"
}
```

---

## 12) Optional improvement — Use a real sensor on the ESP32

Example options:

- DHT22 for temperature and humidity
- LDR for light level
- PIR for motion
- HC-SR04 for distance

Recommended practice:

- keep the JSON structure stable
- include `device_id`
- include measurement units in documentation
- include a gateway timestamp on the Raspberry Pi
- optionally include RSSI or battery voltage later

Example extended payload:

```json
{
  "device_id": "esp32-node-01",
  "temperature_c": 28.4,
  "humidity_pct": 74.1,
  "light_adc": 512,
  "pir": 1,
  "distance_cm": 84.6,
  "gateway_ts": "2026-03-25T10:30:00.000000+00:00"
}
```

---

## 13) Recommended production improvements

For a classroom demo, the simple version is enough. For a stronger design, add these:

### ESP32 side

- reconnect logic if Bluetooth disconnects
- buffering if the gateway is unavailable
- sensor read validation
- sequence number for each packet

### Raspberry Pi side

- structured logging
- local file buffering when internet is down
- retry logic for Pub/Sub publish failures
- systemd service for auto-start
- input schema validation

### Google Cloud side

- Pub/Sub topic per team or per sensor class
- BigQuery subscription or Dataflow pipeline downstream
- dashboard using Looker Studio or a custom app

Google Cloud also documents using Dataflow to stream messages from Pub/Sub into downstream processing pipelines. citeturn638984search18

---

## 14) Run the gateway automatically with systemd

Create `/etc/systemd/system/rpi-bt-pubsub.service`

```ini
[Unit]
Description=Raspberry Pi Bluetooth to Google PubSub Gateway
After=network-online.target bluetooth.target
Wants=network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/iot_gateway
Environment=GOOGLE_APPLICATION_CREDENTIALS=/home/pi/gcp-keys/rpi-gateway-key.json
ExecStart=/usr/bin/python3 /home/pi/iot_gateway/rpi_bt_to_pubsub.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Then:

```bash
sudo systemctl daemon-reload
sudo systemctl enable rpi-bt-pubsub.service
sudo systemctl start rpi-bt-pubsub.service
sudo systemctl status rpi-bt-pubsub.service
```

---

## 15) Troubleshooting

## ESP32 not visible in Bluetooth scan

Check:

- ESP32 code is running
- Bluetooth started successfully
- board has enough power

## Pairing fails

Try:

- remove old pairing on Pi
- reboot ESP32
- re-run `bluetoothctl`

## `/dev/rfcomm0` not found

Try:

- bind the device again with `rfcomm`
- reconnect in `bluetoothctl`
- check that the Bluetooth MAC address is correct

## Pub/Sub publish fails

Check:

- `GOOGLE_APPLICATION_CREDENTIALS` points to the correct JSON key
- service account has `roles/pubsub.publisher`
- topic name and project ID are correct
- Raspberry Pi has internet access

## JSON parse errors on Raspberry Pi

Check:

- each message ends with newline
- ESP32 sends valid JSON
- no extra debug text is sent on Bluetooth

A good practice is:

- send debug messages to `Serial`
- send only JSON payloads to `SerialBT`

---

## 16) Suggested folder structure

### ESP32 project

```text
esp32_bt_sensor/
├── platformio.ini
└── src/
    └── main.cpp
```

### Raspberry Pi gateway project

```text
iot_gateway/
├── rpi_bt_to_pubsub.py
└── requirements.txt
```

Example `requirements.txt`:

```text
google-cloud-pubsub>=2.0.0
```

---

## 17) End-to-end test plan

### Test 1 — ESP32 local output

Open PlatformIO serial monitor and verify JSON lines appear every 5 seconds.

### Test 2 — Raspberry Pi Bluetooth receive

Confirm the gateway prints:

```text
RX: {"device_id":"esp32-node-01",...}
```

### Test 3 — Pub/Sub publish

Confirm the gateway prints:

```text
Published message ID: ...
```

### Test 4 — Cloud verification

Pull the messages from the test subscription.

### Test 5 — Failure test

Disconnect the internet on the Pi and confirm the script reports publish failure.

---

## 18) Next step ideas

After this tutorial works, the next logical extensions are:

1. Pub/Sub → BigQuery for storage
2. BigQuery → dashboard
3. dashboard → command topic → Raspberry Pi → ESP32 actuator control
4. multiple ESP32 nodes sending to one Raspberry Pi gateway
5. switch from simulated data to real sensors

---

## 19) Summary

In this tutorial, you built a gateway-style IoT pipeline:

- **ESP32** sends sensor data over **Bluetooth Classic**
- **Raspberry Pi** receives the data through **RFCOMM**
- **Raspberry Pi** publishes the data to **Google Cloud Pub/Sub**

This architecture is simple, practical, and suitable for teaching because it separates:

- device sensing
- local gateway communication
- cloud ingestion

That separation also makes it easier to extend the system later into storage, analytics, ML, and dashboards.

---

## References

- Google Cloud Pub/Sub overview: https://cloud.google.com/pubsub/docs/pubsub-basics
- Google Cloud Pub/Sub client libraries: https://cloud.google.com/pubsub/docs/reference/libraries
- Google Cloud Pub/Sub Python client reference: https://cloud.google.com/python/docs/reference/pubsub/latest
- Google Cloud pull subscriptions: https://cloud.google.com/pubsub/docs/pull
- Google Cloud create pull subscriptions: https://cloud.google.com/pubsub/docs/create-subscription
- Google Cloud Dataflow stream from Pub/Sub: https://cloud.google.com/pubsub/docs/stream-messages-dataflow

