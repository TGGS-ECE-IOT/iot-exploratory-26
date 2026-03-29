from __future__ import annotations

from typing import Any

from gateway.models import CanonicalEvent


def parse_incoming(topic: str, payload: dict[str, Any], qos: int, retained: bool) -> CanonicalEvent | None:
    parts = topic.split("/")

    if len(parts) >= 7 and parts[0] == "tggs" and parts[1] == "v1":
        # tggs/v1/{site_id}/{node_id}/{direction}/{channel...}
        site_id = parts[2]
        node_id = parts[3]
        direction = parts[4]
        channel = "/".join(parts[5:])
        if direction != "up":
            return None
        return CanonicalEvent(
            source="sensor-node",
            category=_sensor_category(channel, payload),
            topic=topic,
            qos=qos,
            retained=retained,
            site_id=site_id,
            node_id=node_id,
            msg_id=payload.get("msg_id"),
            command_id=payload.get("payload", {}).get("command_id"),
            payload=payload.get("payload", {}),
            raw=payload,
        )

    if len(parts) == 4 and parts[0] == "iot" and parts[1] == "actuator":
        # iot/actuator/{deviceId}/{channel}
        device_id = parts[2]
        channel = parts[3]
        if channel == "cmd":
            return None
        return CanonicalEvent(
            source="actuator-node",
            category=f"actuator.{channel}",
            topic=topic,
            qos=qos,
            retained=retained,
            device_id=device_id,
            msg_id=payload.get("msg_id"),
            payload=payload,
            raw=payload,
        )

    return None


def _sensor_category(channel: str, payload: dict[str, Any]) -> str:
    packet_type = payload.get("type")
    if isinstance(packet_type, str) and packet_type:
        return packet_type
    return channel.replace("/", ".")
