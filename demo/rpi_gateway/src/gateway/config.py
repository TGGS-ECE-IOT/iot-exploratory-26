from __future__ import annotations

from pathlib import Path
from typing import Any
import tomllib

from pydantic import BaseModel, Field


class MQTTConfig(BaseModel):
    host: str = "127.0.0.1"
    port: int = 1883
    username: str | None = None
    password: str | None = None
    client_id: str = "rpi-gateway"
    keepalive_sec: int = 60


class TopicConfig(BaseModel):
    sensor_subscriptions: list[str] = Field(
        default_factory=lambda: [
            "tggs/v1/+/+/up/telemetry/periodic",
            "tggs/v1/+/+/up/event/pir",
            "tggs/v1/+/+/up/event/pot",
            "tggs/v1/+/+/up/status/playback",
            "tggs/v1/+/+/up/ack",
            "tggs/v1/+/+/up/status/online",
        ]
    )
    actuator_subscriptions: list[str] = Field(
        default_factory=lambda: [
            "iot/actuator/+/ack",
            "iot/actuator/+/status",
            "iot/actuator/+/availability",
        ]
    )
    command_monitor_subscriptions: list[str] = Field(
        default_factory=lambda: [
            "tggs/v1/+/+/down/#",
            "iot/actuator/+/cmd",
        ]
    )


class LoggingConfig(BaseModel):
    directory: str = "./logs"
    app_file: str = "gateway.log"
    message_file: str = "messages.log"
    level: str = "INFO"


class ObservabilityConfig(BaseModel):
    enabled: bool = True
    host: str = "0.0.0.0"
    port: int = 8080


class SpoolConfig(BaseModel):
    enabled: bool = True
    path: str = "./data/spool.db"
    flush_interval_sec: int = 5
    max_batch_size: int = 200


class CloudPubSubConfig(BaseModel):
    enabled: bool = False
    project_id: str = ""
    publish_topic: str = ""
    command_subscription: str = ""


class SimulatorConfig(BaseModel):
    enabled: bool = False
    interval_sec: float = 3.0
    site_id: str = "kmutnb-lab-a"
    sensor_node_id: str = "esp32-01"
    actuator_device_id: str = "esp32-act-01"


class AppConfig(BaseModel):
    mode: str = "gateway"
    mqtt: MQTTConfig = Field(default_factory=MQTTConfig)
    topics: TopicConfig = Field(default_factory=TopicConfig)
    logging: LoggingConfig = Field(default_factory=LoggingConfig)
    observability: ObservabilityConfig = Field(default_factory=ObservabilityConfig)
    spool: SpoolConfig = Field(default_factory=SpoolConfig)
    cloud_pubsub: CloudPubSubConfig = Field(default_factory=CloudPubSubConfig)
    simulator: SimulatorConfig = Field(default_factory=SimulatorConfig)


def load_config(path: str | Path | None) -> AppConfig:
    if path is None:
        return AppConfig()

    p = Path(path)
    if not p.exists():
        raise FileNotFoundError(f"Config file not found: {p}")

    with p.open("rb") as f:
        raw: dict[str, Any] = tomllib.load(f)

    return AppConfig.model_validate(raw)
