from __future__ import annotations

import json
import random
import threading
import time
import uuid

import paho.mqtt.client as mqtt

from gateway.config import AppConfig
from gateway.models import utc_now_iso


class LocalDeviceSimulator:
    def __init__(self, cfg: AppConfig) -> None:
        self._cfg = cfg
        self._running = False
        self._thread: threading.Thread | None = None
        self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="rpi-local-simulator")

        if cfg.mqtt.username:
            self._client.username_pw_set(cfg.mqtt.username, cfg.mqtt.password)

        self._client.on_message = self._on_command

    def start(self) -> None:
        self._client.connect(self._cfg.mqtt.host, self._cfg.mqtt.port, self._cfg.mqtt.keepalive_sec)
        self._client.subscribe(f"tggs/v1/{self._cfg.simulator.site_id}/{self._cfg.simulator.sensor_node_id}/down/#", qos=1)
        self._client.subscribe(f"iot/actuator/{self._cfg.simulator.actuator_device_id}/cmd", qos=1)
        self._client.loop_start()

        self._running = True
        self._thread = threading.Thread(target=self._loop, daemon=True)
        self._thread.start()

    def stop(self) -> None:
        self._running = False
        if self._thread:
            self._thread.join(timeout=2)
        self._client.loop_stop()
        self._client.disconnect()

    def _on_command(self, client: mqtt.Client, userdata: object, msg: mqtt.MQTTMessage) -> None:
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except Exception:
            return
        print(f"[SIM CMD] topic={msg.topic} payload={payload}")

    def _loop(self) -> None:
        site_id = self._cfg.simulator.site_id
        node_id = self._cfg.simulator.sensor_node_id
        device_id = self._cfg.simulator.actuator_device_id

        while self._running:
            sensor_payload = {
                "schema": "tggs.node.packet.v1",
                "msg_id": str(uuid.uuid4()),
                "ts": utc_now_iso(),
                "site_id": site_id,
                "node_id": node_id,
                "type": "telemetry.periodic",
                "payload": {
                    "period_sec": int(self._cfg.simulator.interval_sec),
                    "sensors": {
                        "dht22": {
                            "temp_c": round(random.uniform(26.0, 32.0), 1),
                            "humidity_pct": round(random.uniform(55.0, 72.0), 1),
                        },
                        "ultrasonic": {"distance_cm": round(random.uniform(20.0, 80.0), 1)},
                    },
                    "device": {
                        "rssi_dbm": -60,
                        "uptime_sec": int(time.time()) % 100000,
                        "vbat_mv": 4980,
                    },
                },
            }
            self._client.publish(
                f"tggs/v1/{site_id}/{node_id}/up/telemetry/periodic",
                json.dumps(sensor_payload),
                qos=0,
            )

            actuator_status = {
                "v": 1,
                "device_id": device_id,
                "ts": utc_now_iso(),
                "net": {"wifi": "connected", "mqtt": "connected", "rssi": -58, "ip": "192.168.1.45"},
                "actuators": {
                    "fan": {"on": random.choice([True, False])},
                    "servo": {"position_deg": random.choice([0, 45, 90, 135, 180])},
                    "led": {"red": False, "yellow": False, "green": True},
                    "buzzer": {"active": False},
                    "oled": {"mode": "status", "last_cmd_text": "simulated"},
                },
                "meta": {"uptime_s": int(time.time()) % 10000, "last_cmd_id": "", "last_error": ""},
            }
            self._client.publish(f"iot/actuator/{device_id}/status", json.dumps(actuator_status), qos=1, retain=True)

            time.sleep(self._cfg.simulator.interval_sec)
