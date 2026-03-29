from __future__ import annotations

import json
import logging
from logging.handlers import RotatingFileHandler
from pathlib import Path
from typing import Any

from gateway.config import LoggingConfig


def setup_logging(cfg: LoggingConfig) -> tuple[logging.Logger, logging.Logger]:
    log_dir = Path(cfg.directory)
    log_dir.mkdir(parents=True, exist_ok=True)

    app_logger = logging.getLogger("gateway")
    app_logger.setLevel(getattr(logging, cfg.level.upper(), logging.INFO))
    app_logger.handlers.clear()

    app_handler = RotatingFileHandler(log_dir / cfg.app_file, maxBytes=2_000_000, backupCount=5)
    app_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(name)s %(message)s"))
    app_logger.addHandler(app_handler)

    stream_handler = logging.StreamHandler()
    stream_handler.setFormatter(logging.Formatter("%(asctime)s %(levelname)s %(message)s"))
    app_logger.addHandler(stream_handler)

    message_logger = logging.getLogger("gateway.messages")
    message_logger.setLevel(logging.INFO)
    message_logger.handlers.clear()
    msg_handler = RotatingFileHandler(log_dir / cfg.message_file, maxBytes=5_000_000, backupCount=10)
    msg_handler.setFormatter(logging.Formatter("%(message)s"))
    message_logger.addHandler(msg_handler)

    return app_logger, message_logger


def log_message(logger: logging.Logger, direction: str, topic: str, payload: dict[str, Any], meta: dict[str, Any] | None = None) -> None:
    body = {
        "direction": direction,
        "topic": topic,
        "payload": payload,
    }
    if meta:
        body["meta"] = meta
    logger.info(json.dumps(body, separators=(",", ":"), ensure_ascii=True))
