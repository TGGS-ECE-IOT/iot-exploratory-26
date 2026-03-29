from __future__ import annotations

import logging
import threading
import time

from gateway.cloud_pubsub import PubSubBridge
from gateway.config import AppConfig
from gateway.logging_setup import log_message
from gateway.metrics import Metrics
from gateway.models import CanonicalEvent, CloudCommand
from gateway.mqtt_bridge import MQTTBridge
from gateway.observability import ObservabilityServer
from gateway.spool import SpoolStore


class GatewayService:
    def __init__(
        self,
        cfg: AppConfig,
        logger: logging.Logger,
        message_logger: logging.Logger,
    ) -> None:
        self._cfg = cfg
        self._logger = logger
        self._message_logger = message_logger
        self._metrics = Metrics()
        self._mqtt = MQTTBridge(cfg.mqtt, cfg.topics, logger, self._on_event)
        self._cloud = PubSubBridge(cfg.cloud_pubsub, logger)
        self._spool = SpoolStore(cfg.spool.path) if cfg.spool.enabled else None
        self._obs = None
        self._stop_event = threading.Event()
        self._flush_thread: threading.Thread | None = None
        self._command_stream = None

        if cfg.observability.enabled:
            self._obs = ObservabilityServer(cfg.observability.host, cfg.observability.port, self._metrics)

    def start(self) -> None:
        self._logger.info("Starting gateway service mode=%s", self._cfg.mode)

        self._cloud.start()
        self._mqtt.start()

        if self._obs:
            self._obs.start()
            self._logger.info("Observability server on http://%s:%s", self._cfg.observability.host, self._cfg.observability.port)

        self._command_stream = self._cloud.subscribe_commands(self._on_cloud_command)

        self._flush_thread = threading.Thread(target=self._flush_loop, daemon=True)
        self._flush_thread.start()

    def stop(self) -> None:
        self._stop_event.set()
        if self._flush_thread:
            self._flush_thread.join(timeout=2)
        if self._command_stream is not None:
            self._command_stream.cancel()

        self._mqtt.stop()
        if self._obs:
            self._obs.stop()
        self._cloud.close()
        if self._spool:
            self._spool.close()
        self._logger.info("Gateway service stopped")

    def wait_forever(self) -> None:
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            self._logger.info("Shutdown signal received")
            self.stop()

    def _on_event(self, event: CanonicalEvent) -> None:
        self._metrics.inc("mqtt_ingress_total")
        log_message(
            self._message_logger,
            direction="mqtt_in",
            topic=event.topic,
            payload=event.raw,
            meta={"source": event.source, "category": event.category},
        )

        if self._cfg.cloud_pubsub.enabled:
            try:
                self._cloud.publish_event(event)
                self._metrics.inc("pubsub_publish_ok_total")
            except Exception as exc:
                self._metrics.inc("pubsub_publish_fail_total")
                self._logger.warning("Cloud publish failed, spooling event: %s", exc)
                if self._spool:
                    self._spool.enqueue(event)
        else:
            if self._spool:
                self._spool.enqueue(event)
                self._metrics.inc("spooled_total")

    def _on_cloud_command(self, command: CloudCommand) -> None:
        self._metrics.inc("cloud_command_total")
        topic = self._mqtt.publish_command(command)
        log_message(
            self._message_logger,
            direction="mqtt_out",
            topic=topic,
            payload=command.payload,
            meta={"target": command.target},
        )
        self._logger.info("Forwarded cloud command target=%s topic=%s", command.target, topic)

    def _flush_loop(self) -> None:
        while not self._stop_event.is_set():
            if self._cfg.cloud_pubsub.enabled and self._spool:
                self._flush_spool_once()
            time.sleep(self._cfg.spool.flush_interval_sec)

    def _flush_spool_once(self) -> None:
        if not self._spool:
            return
        batch = self._spool.dequeue_batch(self._cfg.spool.max_batch_size)
        if not batch:
            return

        sent_ids: list[int] = []
        for spool_id, event in batch:
            try:
                self._cloud.publish_event(event)
                sent_ids.append(spool_id)
                self._metrics.inc("spool_flush_ok_total")
            except Exception:
                self._metrics.inc("spool_flush_fail_total")
                break

        self._spool.delete_ids(sent_ids)
