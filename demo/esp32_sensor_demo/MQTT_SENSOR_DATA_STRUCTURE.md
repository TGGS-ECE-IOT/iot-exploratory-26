# MQTT Data Structure Design (ESP32 Sensor Node)

This document defines the MQTT topic and JSON packet structure for the ESP32 node, Raspberry Pi gateway, and cloud integration.

## Scope

- Cloud commands to control passive buzzer, RGB LEDs (Red/Yellow/Green), and OLED display.
- Song playback support for:
  - `happy_birthday`
  - `jingle_bells`
  - `loy_krathong`
- Two telemetry groups:
  - Periodic sensors: DHT22, air quality, ultrasonic distance, LDR
  - Trigger/change sensors: PIR, slider POT (500 ms debounce)

## Topic Namespace

Use this namespace pattern:

`tggs/v1/{site_id}/{node_id}/{direction}/{channel}`

Where:

- `{direction}` is `up` (device -> gateway/cloud) or `down` (gateway/cloud -> device)
- `{channel}` is one of the channels below

### Uplink Topics (device -> gateway/cloud)

- `tggs/v1/{site_id}/{node_id}/up/telemetry/periodic`
- `tggs/v1/{site_id}/{node_id}/up/event/pir`
- `tggs/v1/{site_id}/{node_id}/up/event/pot`
- `tggs/v1/{site_id}/{node_id}/up/status/playback`
- `tggs/v1/{site_id}/{node_id}/up/ack`
- `tggs/v1/{site_id}/{node_id}/up/status/online` (LWT/online state)

### Downlink Topics (gateway/cloud -> device)

- `tggs/v1/{site_id}/{node_id}/down/cmd/playback`
- `tggs/v1/{site_id}/{node_id}/down/cmd/display`
- `tggs/v1/{site_id}/{node_id}/down/cmd/led`
- `tggs/v1/{site_id}/{node_id}/down/cfg` (optional retained config)

## Common JSON Envelope

All packets use a shared envelope with type-specific `payload`.

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "01JXYZABC123",
  "ts": "2026-03-30T08:15:27Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "telemetry.periodic",
  "payload": {}
}
```

### Envelope Field Notes

- `schema`: fixed schema identifier.
- `msg_id`: unique message id (UUID/ULID/time-based id).
- `ts`: UTC timestamp in ISO 8601 format.
- `type`: packet semantic type (`cmd.playback`, `event.pot`, etc.).

## Downlink Command Packets

### 1) Playback Command

Topic: `.../down/cmd/playback`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "cmd-1001",
  "ts": "2026-03-30T08:20:00Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "cmd.playback",
  "payload": {
    "command_id": "play-20260330-001",
    "action": "play",
    "song": {
      "song_id": "happy_birthday",
      "variant": "default",
      "tempo_bpm": 112,
      "repeat": 1
    },
    "sync": {
      "led_mode": "rhythm_rgb_cycle",
      "oled_mode": "lyrics",
      "start_at": "immediate"
    },
    "priority": 5,
    "ttl_sec": 30
  }
}
```

#### Playback action values

- `play`
- `pause`
- `resume`
- `stop`

#### Song id values

- `happy_birthday`
- `jingle_bells`
- `loy_krathong`
- `custom` (optional future extension)

### 2) Display Command

Topic: `.../down/cmd/display`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "disp-88",
  "ts": "2026-03-30T08:20:30Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "cmd.display",
  "payload": {
    "command_id": "disp-88",
    "mode": "normal",
    "layout": {
      "top_status_px": 16,
      "bottom_mode": "sensor_values"
    },
    "override_sec": 0
  }
}
```

Mode examples:

- `normal`
- `lyrics`
- `message`

## Uplink ACK and Playback Status

### 1) Command ACK

Topic: `.../up/ack`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "ack-1001",
  "ts": "2026-03-30T08:20:01Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "ack.command",
  "payload": {
    "command_id": "play-20260330-001",
    "status": "accepted",
    "detail": "queued"
  }
}
```

ACK status values:

- `accepted`
- `rejected`
- `executing`
- `completed`
- `failed`

### 2) Playback Progress Status

Topic: `.../up/status/playback`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "play-stat-302",
  "ts": "2026-03-30T08:20:10Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "status.playback",
  "payload": {
    "command_id": "play-20260330-001",
    "state": "playing",
    "song_id": "loy_krathong",
    "line_index": 3,
    "note_index": 12,
    "progress_pct": 42
  }
}
```

## Sensor Telemetry Packets

## Group 1: Periodic sensors

Topic: `.../up/telemetry/periodic`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "tele-2001",
  "ts": "2026-03-30T08:21:00Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "telemetry.periodic",
  "payload": {
    "period_sec": 15,
    "sensors": {
      "dht22": { "temp_c": 28.6, "humidity_pct": 67.2 },
      "air_quality": { "raw": 1480, "aq_index": 72 },
      "ultrasonic": { "distance_cm": 53.4 },
      "ldr": { "raw": 2050, "lux_est": 340 }
    },
    "device": {
      "rssi_dbm": -61,
      "uptime_sec": 12345,
      "vbat_mv": 4980
    }
  }
}
```

## Group 2: Trigger/change sensors

### PIR event

Topic: `.../up/event/pir`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "pir-3001",
  "ts": "2026-03-30T08:22:03Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "event.pir",
  "payload": {
    "event": "motion_detected",
    "value": 1,
    "edge": "rising",
    "cooldown_ms": 2000
  }
}
```

Optional clear event can publish `value: 0` with `edge: falling`.

### Slider POT event (500 ms debounce)

Topic: `.../up/event/pot`

```json
{
  "schema": "tggs.node.packet.v1",
  "msg_id": "pot-4001",
  "ts": "2026-03-30T08:22:07Z",
  "site_id": "kmutnb-lab-a",
  "node_id": "esp32-01",
  "type": "event.pot",
  "payload": {
    "value_raw": 2870,
    "value_pct": 70.1,
    "debounce_ms": 500,
    "change": {
      "prev_pct": 64.8,
      "delta_pct": 5.3
    }
  }
}
```

Recommended publish condition:

- value is stable for at least `500 ms`
- and absolute change is at least `2%`

## Playback and OLED/LED Behavior Contract

When `cmd.playback` is accepted:

1. Passive buzzer plays selected song melody.
2. Red/Yellow/Green LEDs blink by rhythm (`led_mode`).
3. OLED switches to lyrics mode and displays line/note progression.
4. Node publishes ACK and playback progress updates.
5. On completion, node returns OLED to prior mode unless overridden.

## QoS and Retain Recommendation

- `down/cmd/*`: QoS 1, retain false
- `up/ack`: QoS 1, retain false
- `up/status/playback`: QoS 1, retain false
- `up/event/*`: QoS 1, retain false
- `up/telemetry/periodic`: QoS 0 (or QoS 1 if required)
- `down/cfg`: QoS 1, retain true
- `up/status/online` (LWT): retained online/offline state

## Validation and Implementation Notes

- Keep packet names and enum values stable once deployed.
- Use strict JSON parsing with defaults for optional fields.
- Reject commands missing `command_id`, `action`, or invalid `song_id`.
- Include `command_id` in all ACK/status packets for traceability.
- Use UTC timestamps from NTP-synced clock.
