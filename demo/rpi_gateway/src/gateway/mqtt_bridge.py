from __future__ import annotations

import json
import logging
from typing import Callable

import paho.mqtt.client as mqtt

from gateway.config import MQTTConfig, TopicConfig
from gateway.models import CloudCommand
from gateway.topic_parser import parse_incoming


class MQTTBridge:
    def __init__(
        self,
        mqtt_cfg: MQTTConfig,
        topic_cfg: TopicConfig,
        logger: logging.Logger,
        on_event: Callable,
    ) -> None:
        self._mqtt_cfg = mqtt_cfg
        self._topic_cfg = topic_cfg
        self._logger = logger
        self._on_event = on_event
        self._client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=mqtt_cfg.client_id)

        if mqtt_cfg.username:
            self._client.username_pw_set(mqtt_cfg.username, mqtt_cfg.password)

        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message
        self._client.on_disconnect = self._on_disconnect

    def start(self) -> None:
        self._client.connect(self._mqtt_cfg.host, self._mqtt_cfg.port, self._mqtt_cfg.keepalive_sec)
        self._client.loop_start()

    def stop(self) -> None:
        self._client.loop_stop()
        self._client.disconnect()

    def _on_connect(self, client: mqtt.Client, userdata: object, flags: dict, reason_code: object, properties: object) -> None:
        if getattr(reason_code, "is_failure", False):
            self._logger.error("MQTT connect failed: %s", reason_code)
            return
        self._logger.info("Connected to MQTT broker %s:%s", self._mqtt_cfg.host, self._mqtt_cfg.port)
        for topic in self._topic_cfg.sensor_subscriptions + self._topic_cfg.actuator_subscriptions:
            client.subscribe(topic, qos=1)
            self._logger.info("Subscribed %s", topic)

    def _on_disconnect(self, client: mqtt.Client, userdata: object, flags: dict, reason_code: object, properties: object) -> None:
        self._logger.warning("Disconnected from MQTT broker: %s", reason_code)

    def _on_message(self, client: mqtt.Client, userdata: object, msg: mqtt.MQTTMessage) -> None:
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except Exception as exc:
            self._logger.error("Invalid JSON on topic=%s err=%s", msg.topic, exc)
            return

        event = parse_incoming(msg.topic, payload, msg.qos, msg.retain)
        if event is None:
            return
        self._on_event(event)

    def publish_command(self, command: CloudCommand) -> str:
        topic = command.topic or self._derive_topic(command)
        payload = command.payload
        self._client.publish(topic, json.dumps(payload, separators=(",", ":")), qos=command.qos, retain=command.retain)
        return topic

    @staticmethod
    def _derive_topic(command: CloudCommand) -> str:
        if command.target == "actuator":
            if not command.device_id:
                raise ValueError("device_id is required for actuator command")
            return f"iot/actuator/{command.device_id}/cmd"

        if command.target == "sensor":
            if not command.site_id or not command.node_id:
                raise ValueError("site_id and node_id are required for sensor command")
            packet_type = command.payload.get("type", "")
            mapping = {
                "cmd.playback": "down/cmd/playback",
                "cmd.display": "down/cmd/display",
                "cmd.led": "down/cmd/led",
            }
            channel = mapping.get(packet_type, "down/cfg")
            return f"tggs/v1/{command.site_id}/{command.node_id}/{channel}"

        raise ValueError(f"Unknown command target: {command.target}")
