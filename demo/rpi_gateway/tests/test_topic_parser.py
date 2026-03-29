from gateway.topic_parser import parse_incoming


def test_parse_sensor_periodic_topic() -> None:
    payload = {
        "msg_id": "abc",
        "type": "telemetry.periodic",
        "payload": {"period_sec": 15},
    }
    event = parse_incoming("tggs/v1/site-1/esp32-01/up/telemetry/periodic", payload, qos=0, retained=False)
    assert event is not None
    assert event.source == "sensor-node"
    assert event.site_id == "site-1"
    assert event.node_id == "esp32-01"
    assert event.category == "telemetry.periodic"


def test_parse_actuator_status_topic() -> None:
    payload = {"v": 1, "device_id": "esp32-act-01"}
    event = parse_incoming("iot/actuator/esp32-act-01/status", payload, qos=1, retained=True)
    assert event is not None
    assert event.source == "actuator-node"
    assert event.device_id == "esp32-act-01"
    assert event.category == "actuator.status"
