# MQTT Design (v1)

This document defines the MQTT topic layout and JSON payload contract for the ESP32 actuator device.

## Scope

- Device: ESP32 actuator node
- Cloud side: Google Cloud MQTT client/broker integration
- Controlled outputs:
  - Fan (relay on/off)
  - Servo (position)
  - LEDs (red, yellow, green)
  - Active buzzer (beep)
  - OLED message display
- Control style: one command per MQTT message
- Device response: publish full current status after command handling

## Topic Structure

Use per-device topic namespaces:

- Command subscribe (cloud -> device):
  - `iot/actuator/{deviceId}/cmd`
- Command acknowledgement publish (device -> cloud):
  - `iot/actuator/{deviceId}/ack`
- Full status publish (device -> cloud):
  - `iot/actuator/{deviceId}/status`
- Availability/LWT publish (device -> cloud):
  - `iot/actuator/{deviceId}/availability`

Example with `deviceId = esp32-act-01`:

- `iot/actuator/esp32-act-01/cmd`
- `iot/actuator/esp32-act-01/ack`
- `iot/actuator/esp32-act-01/status`
- `iot/actuator/esp32-act-01/availability`

## QoS / Retain Policy

- `cmd`: QoS 1, retained false
- `ack`: QoS 1, retained false
- `status`: QoS 1, retained true
- `availability`: QoS 1, retained true (`online`/`offline`)

## Command Payload

Command messages are JSON and contain a single target/action at a time.

```json
{
  "v": 1,
  "msg_id": "d5a5c7f2-9f9d-4d66-9fbe-2f3f1a3f9a91",
  "ts": "2026-03-30T10:20:15Z",
  "target": "fan",
  "action": "set",
  "params": {
    "on": true
  }
}
```

### Required fields

- `v` (int): protocol version, currently `1`
- `msg_id` (string): unique command ID
- `target` (string): one of `fan | servo | led | buzzer | oled | system`
- `action` (string): action for the target

### Supported targets/actions

- `fan`
  - `action: set`
  - `params.on` (bool)

- `servo`
  - `action: set`
  - `params.position_deg` (int, 0..180)

- `led`
  - `action: set`
  - Either:
    - `params.color` = `red | yellow | green | off`
  - Or:
    - `params.red` / `params.yellow` / `params.green` (bool)

- `buzzer`
  - `action: beep`
  - `params.count` (int >= 1)
  - `params.on_ms` (int)
  - `params.off_ms` (int)

- `oled`
  - `action: show`
  - `params.line1`..`params.line4` (string)

- `system`
  - `action: get_status`

## ACK Payload

Each valid/invalid command produces one ack with matching `msg_id`.

```json
{
  "v": 1,
  "msg_id": "d5a5c7f2-9f9d-4d66-9fbe-2f3f1a3f9a91",
  "device_id": "esp32-act-01",
  "accepted": true,
  "code": "OK",
  "message": "Command applied",
  "ts": "2026-03-30T10:20:15Z"
}
```

Failure example:

```json
{
  "v": 1,
  "msg_id": "d5a5c7f2-9f9d-4d66-9fbe-2f3f1a3f9a91",
  "device_id": "esp32-act-01",
  "accepted": false,
  "code": "INVALID_PARAM",
  "message": "position_deg out of range (0..180)",
  "ts": "2026-03-30T10:20:15Z"
}
```

## Status Payload (Full Snapshot)

Device publishes full state in one message.

```json
{
  "v": 1,
  "device_id": "esp32-act-01",
  "ts": "2026-03-30T10:20:16Z",
  "net": {
    "wifi": "connected",
    "mqtt": "connected",
    "rssi": -58,
    "ip": "192.168.1.45"
  },
  "actuators": {
    "fan": { "on": true },
    "servo": { "position_deg": 90 },
    "led": { "red": false, "yellow": false, "green": true },
    "buzzer": { "active": false },
    "oled": { "mode": "status", "last_cmd_text": "fan set on" }
  },
  "meta": {
    "uptime_s": 1240,
    "last_cmd_id": "d5a5c7f2-9f9d-4d66-9fbe-2f3f1a3f9a91",
    "last_error": ""
  }
}
```

## OLED Behavior

- On MQTT command receive:
  - briefly show command summary (e.g. `CMD: FAN ON`)
- Then return to status layout:
  - left side: WiFi/MQTT/Time
  - right side: fan/servo/LED current state

## Validation Rules

- Ignore unknown fields for forward compatibility
- Reject unknown `target` or `action` with `accepted=false`
- Validate numeric ranges (e.g. servo 0..180)
- Deduplicate repeated `msg_id` within a short cache window
- Publish `ack` first, then publish updated `status`

## Versioning

- `v` is the protocol version
- Backward incompatible changes must increment `v`
- Keep `v=1` stable for cloud and device integration
