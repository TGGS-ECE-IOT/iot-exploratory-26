from __future__ import annotations

import argparse
import random
import sys
import time
import uuid

from gateway.cloud_pubsub import PubSubBridge
from gateway.config import AppConfig, load_config
from gateway.logging_setup import setup_logging
from gateway.models import CanonicalEvent, utc_now_iso
from gateway.service import GatewayService
from gateway.simulator import LocalDeviceSimulator


def _load(args: argparse.Namespace) -> tuple[AppConfig, object, object]:
    cfg = load_config(args.config)
    app_logger, message_logger = setup_logging(cfg.logging)
    return cfg, app_logger, message_logger


def cmd_run(args: argparse.Namespace) -> int:
    cfg, app_logger, message_logger = _load(args)
    service = GatewayService(cfg, app_logger, message_logger)
    service.start()
    service.wait_forever()
    return 0


def cmd_offline_demo(args: argparse.Namespace) -> int:
    cfg, app_logger, message_logger = _load(args)
    cfg.cloud_pubsub.enabled = False
    cfg.simulator.enabled = True
    if args.interval is not None:
        cfg.simulator.interval_sec = args.interval

    service = GatewayService(cfg, app_logger, message_logger)
    simulator = LocalDeviceSimulator(cfg)
    service.start()
    simulator.start()

    app_logger.info("Offline demo started. Press Ctrl+C to stop.")
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass
    finally:
        simulator.stop()
        service.stop()
    return 0


def cmd_pubsub_sim(args: argparse.Namespace) -> int:
    cfg, app_logger, _ = _load(args)
    cfg.cloud_pubsub.enabled = True
    if not cfg.cloud_pubsub.project_id or not cfg.cloud_pubsub.publish_topic:
        app_logger.error("Pub/Sub config requires project_id and publish_topic")
        return 2

    cloud = PubSubBridge(cfg.cloud_pubsub, app_logger)
    cloud.start()

    app_logger.info("Publishing %d simulated events to Pub/Sub", args.count)
    for i in range(args.count):
        event = CanonicalEvent(
            source="rpi-simulator",
            category="telemetry.periodic",
            topic="sim/local",
            qos=0,
            site_id=cfg.simulator.site_id,
            node_id=cfg.simulator.sensor_node_id,
            msg_id=str(uuid.uuid4()),
            payload={
                "index": i,
                "temp_c": round(random.uniform(25.0, 33.0), 1),
                "humidity_pct": round(random.uniform(50.0, 75.0), 1),
                "ts": utc_now_iso(),
            },
            raw={"simulated": True},
        )
        cloud.publish_event(event)
        app_logger.info("Sent simulated event index=%s", i)
        time.sleep(args.interval)

    cloud.close()
    app_logger.info("Simulation done")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="RPi IoT Gateway")
    parser.add_argument("--config", default="config/gateway.toml", help="Path to TOML config")

    sub = parser.add_subparsers(dest="command", required=True)

    run_cmd = sub.add_parser("run", help="Run gateway service")
    run_cmd.set_defaults(func=cmd_run)

    demo_cmd = sub.add_parser("offline-demo", help="Run local offline demo")
    demo_cmd.add_argument("--interval", type=float, default=None, help="Simulator publish interval seconds")
    demo_cmd.set_defaults(func=cmd_offline_demo)

    pubsim_cmd = sub.add_parser("pubsub-sim", help="Publish simulated data to Google Pub/Sub")
    pubsim_cmd.add_argument("--count", type=int, default=10)
    pubsim_cmd.add_argument("--interval", type=float, default=1.0)
    pubsim_cmd.set_defaults(func=cmd_pubsub_sim)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    rc = args.func(args)
    raise SystemExit(rc)


if __name__ == "__main__":
    main()
