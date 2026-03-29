from __future__ import annotations

import json
import logging
from typing import Callable

from gateway.config import CloudPubSubConfig
from gateway.models import CanonicalEvent, CloudCommand


class PubSubBridge:
    def __init__(self, cfg: CloudPubSubConfig, logger: logging.Logger) -> None:
        self._cfg = cfg
        self._logger = logger
        self._publisher = None
        self._subscriber = None
        self._topic_path = ""
        self._subscription_path = ""

    def start(self) -> None:
        if not self._cfg.enabled:
            return
        try:
            from google.cloud import pubsub_v1
        except Exception as exc:  # pragma: no cover
            raise RuntimeError("google-cloud-pubsub is not available") from exc

        self._publisher = pubsub_v1.PublisherClient()
        self._subscriber = pubsub_v1.SubscriberClient()
        self._topic_path = self._publisher.topic_path(self._cfg.project_id, self._cfg.publish_topic)
        if self._cfg.command_subscription:
            self._subscription_path = self._subscriber.subscription_path(
                self._cfg.project_id,
                self._cfg.command_subscription,
            )
        self._logger.info("Pub/Sub bridge initialized")

    def publish_event(self, event: CanonicalEvent) -> None:
        if not self._cfg.enabled:
            raise RuntimeError("Pub/Sub is disabled")
        if self._publisher is None:
            raise RuntimeError("Pub/Sub bridge not started")

        data = event.model_dump_json().encode("utf-8")
        future = self._publisher.publish(
            self._topic_path,
            data,
            source=event.source,
            category=event.category,
        )
        future.result(timeout=10)

    def subscribe_commands(self, callback: Callable[[CloudCommand], None]) -> object | None:
        if not self._cfg.enabled or not self._subscription_path:
            return None
        if self._subscriber is None:
            raise RuntimeError("Pub/Sub bridge not started")

        def _on_message(message: object) -> None:
            data = getattr(message, "data", b"{}")
            try:
                obj = json.loads(data.decode("utf-8"))
                command = CloudCommand.model_validate(obj)
                callback(command)
                message.ack()
            except Exception as exc:
                self._logger.error("Invalid cloud command: %s", exc)
                message.nack()

        return self._subscriber.subscribe(self._subscription_path, callback=_on_message)

    def close(self) -> None:
        if self._publisher is not None:
            self._publisher.stop()
        if self._subscriber is not None:
            self._subscriber.close()
