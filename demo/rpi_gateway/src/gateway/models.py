from __future__ import annotations

from datetime import datetime, timezone
from typing import Any

from pydantic import BaseModel, Field


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


class CanonicalEvent(BaseModel):
    ts: str = Field(default_factory=utc_now_iso)
    source: str
    category: str
    topic: str
    qos: int
    retained: bool = False
    site_id: str | None = None
    node_id: str | None = None
    device_id: str | None = None
    msg_id: str | None = None
    command_id: str | None = None
    payload: dict[str, Any] = Field(default_factory=dict)
    raw: dict[str, Any] = Field(default_factory=dict)


class CloudCommand(BaseModel):
    target: str
    payload: dict[str, Any] = Field(default_factory=dict)
    topic: str | None = None
    qos: int = 1
    retain: bool = False
    site_id: str | None = None
    node_id: str | None = None
    device_id: str | None = None
