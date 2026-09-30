from __future__ import annotations

import pytest

from beehive_gateway.mqtt_protocol import (
    MqttMessageError,
    parse_status_payload,
    parse_flat_status_topic,
    parse_flat_telemetry_topic,
    parse_telemetry_payload,
    parse_telemetry_topic,
)


TOPIC = "beehive/v1/gateways/gw-001/nodes/1/telemetry"
PAYLOAD = {
    "schema_version": 1,
    "gateway_id": "gw-001",
    "node_id": 1,
    "sequence": 528,
    "uptime_ms": 1104446,
    "inside_temperature": 28.26,
    "inside_humidity": 60.63,
    "outside_temperature": 28.30,
    "outside_humidity": 63.41,
    "tilted": False,
    "status_flags": 7,
}


def test_parses_target_topic_and_valid_payload() -> None:
    address = parse_telemetry_topic(TOPIC, "beehive/v1")

    message = parse_telemetry_payload(PAYLOAD, address)

    assert address.gateway_id == "gw-001"
    assert address.node_id == 1
    assert message.reading.sequence_no == 528
    assert message.reading.inside_temperature == 28.26


def test_rejects_non_target_topic() -> None:
    with pytest.raises(MqttMessageError):
        parse_telemetry_topic("beehive/v1/gateways/gw-001/status", "beehive/v1")


def test_preserves_null_measurements() -> None:
    address = parse_telemetry_topic(TOPIC, "beehive/v1")
    payload = {**PAYLOAD, "inside_temperature": None, "tilted": None}

    message = parse_telemetry_payload(payload, address)

    assert message.reading.inside_temperature is None
    assert message.reading.tilted is None


def test_rejects_gateway_or_node_mismatch() -> None:
    address = parse_telemetry_topic(TOPIC, "beehive/v1")

    with pytest.raises(MqttMessageError, match="gateway_id"):
        parse_telemetry_payload({**PAYLOAD, "gateway_id": "gw-002"}, address)
    with pytest.raises(MqttMessageError, match="node_id"):
        parse_telemetry_payload({**PAYLOAD, "node_id": 2}, address)


def test_rejects_nan_and_unreasonable_measurements() -> None:
    address = parse_telemetry_topic(TOPIC, "beehive/v1")

    with pytest.raises(MqttMessageError):
        parse_telemetry_payload({**PAYLOAD, "inside_temperature": float("nan")}, address)
    with pytest.raises(MqttMessageError):
        parse_telemetry_payload({**PAYLOAD, "outside_humidity": 150.0}, address)


def test_rejects_missing_required_field() -> None:
    address = parse_telemetry_topic(TOPIC, "beehive/v1")

    with pytest.raises(MqttMessageError, match="status_flags"):
        parse_telemetry_payload({key: value for key, value in PAYLOAD.items() if key != "status_flags"}, address)


@pytest.mark.parametrize("payload", ["online", b"offline", '{"status":"online"}'])
def test_parses_gateway_status(payload: str | bytes) -> None:
    assert parse_status_payload(payload) in {"online", "offline"}


def test_rejects_unknown_gateway_status() -> None:
    with pytest.raises(MqttMessageError):
        parse_status_payload("unknown")


def test_parses_bemfa_flat_topics() -> None:
    address = parse_flat_telemetry_topic(
        "beehivegw001node001telemetry",
        "beehivegw001node001telemetry",
        "gw-001",
        1,
    )
    assert address.gateway_id == "gw-001"
    assert address.node_id == 1
    assert parse_flat_status_topic("beehivegw001status", "beehivegw001status", "gw-001") == "gw-001"
