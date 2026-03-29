# IoT Exploratory 2026: Workshop Overview

## 1. Objective

This workshop introduces students to a **full-stack IoT system** covering:

- Embedded sensing and actuation (ESP32)
- Edge gateway processing (Raspberry Pi)
- Cloud data pipeline (Google Cloud)
- Real-time dashboard and remote control

The goal is to demonstrate a **complete closed-loop IoT system**:

```
Sensor → Edge Device → Gateway → Cloud → Dashboard → Command → Actuator
```

---

## 2. System Architecture Overview

### High-level architecture

```
[ Sensors & Inputs ]
        ↓
     ESP32-A (Sense Node)
        ↓ (WiFi / MQTT)
     Raspberry Pi 5 (Gateway)
        ↓ (HTTPS / PubSub)
     Google Cloud Platform
        ↓
     Dashboard / Control UI
        ↓
     Raspberry Pi 5
        ↓ (MQTT)
     ESP32-B (Actuator Node)
        ↓
[ Outputs / Actuators / Displays ]
```

---

## 3. System Components

### 3.1 ESP32-A: Sensor Node ("Sense")

Responsible for data acquisition and local input handling.

**Connected devices:**

- DHT22 (temperature, humidity)
- HC-SR04 (distance)
- PIR sensor (motion detection)
- LDR (brightness)
- Potentiometer (analog input)
- Slide potentiometer
- 4x4 keypad
- Push buttons

**Responsibilities:**

- Read sensor values
- Perform basic preprocessing (scaling, filtering)
- Package data into JSON
- Send telemetry to Raspberry Pi

---

### 3.2 ESP32-B: Actuator Node ("Act")

Responsible for output control and user interface display.

**Connected devices:**

- RYG traffic LED
- Passive buzzer
- OLED SSD1306 display
- TM1638 (7-segment + keys)
- 1-ch relay
- 2-ch relay
- 5V fan
- Servo motor (SG90)

**Responsibilities:**

- Receive commands from Raspberry Pi
- Control actuators
- Display status locally
- Send acknowledgement/status back

---

### 3.3 Raspberry Pi 5: Gateway ("Gate")

Acts as the bridge between local devices and the cloud.

**Responsibilities:**

- Run MQTT broker or client
- Receive telemetry from ESP32-A
- Forward data to Google Cloud
- Receive commands from cloud
- Route commands to ESP32-B
- Provide buffering if internet is unavailable

---

### 3.4 Google Cloud Platform

Provides data processing, storage, and visualization.

**Services used:**

- Cloud Run (API layer)
- Pub/Sub (message queue)
- BigQuery (data storage)
- Looker Studio (dashboard)

**Responsibilities:**

- Store telemetry data
- Visualize real-time and historical data
- Provide control interface
- Generate commands to devices

---

## 4. Data Flow

### 4.1 Telemetry Flow

```
ESP32-A → Raspberry Pi → Cloud Run → Pub/Sub → BigQuery → Dashboard
```

Steps:

1. Sensors are read by ESP32-A
2. Data is sent via MQTT to Raspberry Pi
3. Raspberry Pi forwards data to Cloud Run API
4. Data is published to Pub/Sub
5. Stored in BigQuery
6. Visualized in dashboard

---

### 4.2 Command Flow

```
Dashboard → Cloud Run → Pub/Sub → Raspberry Pi → ESP32-B → Actuator
```

Steps:

1. User interacts with dashboard
2. Command is sent to Cloud Run
3. Published to Pub/Sub
4. Raspberry Pi receives command
5. Command forwarded to ESP32-B
6. Actuator executes action

---

## 5. Communication Protocols

### Local Network

- Protocol: MQTT over WiFi
- Purpose: communication between ESP32 and Raspberry Pi

### Cloud Communication

- Protocol: HTTPS
- Purpose: communication between Raspberry Pi and Cloud Run

---

## 6. Message Structure

### Example Telemetry JSON

```json
{
  "device_id": "esp32A",
  "temperature": 30.5,
  "humidity": 65,
  "light": 70,
  "distance": 45,
  "motion": true,
  "pot": 55,
  "mode": "AUTO"
}
```

### Example Command JSON

```json
{
  "target": "esp32B",
  "fan": true,
  "traffic": "GREEN",
  "buzzer": false,
  "servo": "CW"
}
```

---

## 7. Workshop Learning Outcomes

Students will be able to:

- Interface sensors and actuators with ESP32
- Design distributed IoT architecture
- Use MQTT for device communication
- Build cloud-connected data pipelines
- Create dashboards for monitoring and control
- Implement bidirectional IoT communication

---

## 8. Design Principles

### Separation of concerns

- ESP32-A: sensing
- ESP32-B: actuation
- RPi: communication and control

### Edge + Cloud hybrid

- Local processing for real-time response
- Cloud for analytics and visualization

### Scalability

- Each team uses unique topics and datasets
- Architecture can scale to many devices

### Fault tolerance

- Local system works even if cloud is down
- Raspberry Pi buffers data

---

## 9. Suggested Workshop Flow

1. Hardware setup (ESP32-A and ESP32-B)
2. Sensor and actuator testing
3. Local MQTT communication
4. Raspberry Pi gateway setup
5. Cloud integration
6. Dashboard creation
7. End-to-end system demonstration

---

## 10. Summary

This workshop demonstrates a **complete IoT system architecture** from device-level sensing to cloud-based control.

The use of two ESP32 boards simplifies development while still exposing students to:

- Distributed systems
- Real-time communication
- Cloud integration

The Raspberry Pi serves as a practical gateway, enabling a realistic and scalable IoT design suitable for both education and real-world applications.

