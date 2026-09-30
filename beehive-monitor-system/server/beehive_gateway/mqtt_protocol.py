"""Strict parsing for the gateway MQTT topic and JSON contract."""

from __future__ import annotations

import json
import math
import re
from dataclasses import dataclass
from typing import Any, Mapping

from beehive_gateway.protocol import SensorReading


class MqttMessageError(ValueError):
    """Raised when an untrusted MQTT message is not valid for this service."""


@dataclass(frozen=True)
class TelemetryTopic:
    gateway_id: str
    node_id: int


def parse_status_topic(topic: str, topic_prefix: str) -> str:
    """Parse exactly ``{prefix}/gateways/{gateway}/status``."""
    prefix_parts = topic_prefix.strip("/").split("/")
    parts = topic.split("/")
    if len(parts) != len(prefix_parts) + 3 or parts[: len(prefix_parts)] != prefix_parts:
        raise MqttMessageError("topic is outside the configured status namespace")
    offset = len(prefix_parts)
    if parts[offset] != "gateways" or parts[offset + 2] != "status":
        raise MqttMessageError("invalid status topic shape")
    gateway_id = parts[offset + 1]
    if not _ID_PATTERN.fullmatch(gateway_id):
        raise MqttMessageError("invalid gateway_id in topic")
    return gateway_id


def parse_flat_status_topic(topic: str, expected_topic: str, gateway_id: str) -> str:
    """Validate a Bemfa-style single-level status topic."""
    if topic != expected_topic or not expected_topic or not _ID_PATTERN.fullmatch(gateway_id):
        raise MqttMessageError("invalid flat status topic")
    return gateway_id


@dataclass(frozen=True)
class MqttTelemetry:
    gateway_id: str
    reading: SensorReading


_ID_PATTERN = re.compile(r"^[A-Za-z0-9_-]{1,64}$")
_MAX_PAYLOAD_BYTES = 8192


def parse_telemetry_topic(topic: str, topic_prefix: str) -> TelemetryTopic:
    """Parse exactly ``{prefix}/gateways/{gateway}/nodes/{node}/telemetry``."""
    if not isinstance(topic, str):
        raise MqttMessageError("topic must be text")

    prefix_parts = topic_prefix.strip("/").split("/")
    parts = topic.split("/")
    if len(parts) != len(prefix_parts) + 5 or parts[: len(prefix_parts)] != prefix_parts:
        raise MqttMessageError("topic is outside the configured telemetry namespace")

    offset = len(prefix_parts)
    if parts[offset] != "gateways" or parts[offset + 2] != "nodes" or parts[offset + 4] != "telemetry":
        raise MqttMessageError("invalid telemetry topic shape")

    gateway_id = parts[offset + 1]
    node_text = parts[offset + 3]
    if not _ID_PATTERN.fullmatch(gateway_id):
        raise MqttMessageError("invalid gateway_id in topic")
    if not node_text.isdecimal():
        raise MqttMessageError("node_id in topic must be numeric")
    node_id = int(node_text)
    if not 1 <= node_id <= 0xFFFF:
        raise MqttMessageError("node_id in topic is out of range")
    return TelemetryTopic(gateway_id=gateway_id, node_id=node_id)


def parse_flat_telemetry_topic(
    topic: str, expected_topic: str, gateway_id: str, node_id: int
) -> TelemetryTopic:
    """Validate a Bemfa-style single-level telemetry topic."""
    if topic != expected_topic or not expected_topic:
        raise MqttMessageError("invalid flat telemetry topic")
    if not _ID_PATTERN.fullmatch(gateway_id) or not 1 <= node_id <= 0xFFFF:
        raise MqttMessageError("invalid flat telemetry identity")
    return TelemetryTopic(gateway_id=gateway_id, node_id=node_id)


def parse_telemetry_payload(payload: Mapping[str, Any] | str | bytes, address: TelemetryTopic) -> MqttTelemetry:
    """Validate an MQTT payload before it is allowed into storage."""
    value = _decode_json(payload)
    if not isinstance(value, dict):
        raise MqttMessageError("telemetry payload must be a JSON object")

    schema_version = _required_uint(value, "schema_version", 1)
    if schema_version != 1:
        raise MqttMessageError("unsupported schema_version")

    gateway_id = value.get("gateway_id")
    if not isinstance(gateway_id, str) or gateway_id != address.gateway_id:
        raise MqttMessageError("gateway_id does not match topic")

    node_id = _required_uint(value, "node_id", 0xFFFF)
    if node_id != address.node_id or node_id == 0:
        raise MqttMessageError("node_id does not match topic")

    reading = SensorReading(
        node_id=node_id,
        sequence_no=_required_uint(value, "sequence", 0xFFFFFFFF),
        uptime_ms=_required_uint(value, "uptime_ms", 0xFFFFFFFF),
        inside_temperature=_measurement(value, "inside_temperature", -80.0, 100.0),
        inside_humidity=_measurement(value, "inside_humidity", 0.0, 100.0),
        outside_temperature=_measurement(value, "outside_temperature", -80.0, 100.0),
        outside_humidity=_measurement(value, "outside_humidity", 0.0, 100.0),
        tilted=_optional_bool(value, "tilted"),
        status_flags=_required_uint(value, "status_flags", 0xFFFF),
    )
    return MqttTelemetry(gateway_id=gateway_id, reading=reading)


def parse_status_payload(payload: str | bytes | Mapping[str, Any]) -> str:
    """Parse either a plain ``online``/``offline`` value or a status object."""
    if isinstance(payload, bytes):
        try:
            text = payload.decode("utf-8")
        except UnicodeDecodeError as error:
            raise MqttMessageError("status payload must be UTF-8") from error
    elif isinstance(payload, str):
        text = payload
    else:
        text = ""

    text = text.strip()
    status: Any = text
    if text.startswith("{"):
        status_object = _decode_json(text)
        if not isinstance(status_object, dict):
            raise MqttMessageError("status payload must be text or object")
        status = status_object.get("status")

    if status not in {"online", "offline"}:
        raise MqttMessageError("status must be online or offline")
    return status


def _decode_json(payload: Mapping[str, Any] | str | bytes) -> Any:
    if isinstance(payload, Mapping):
        return dict(payload)
    if isinstance(payload, bytes):
        if len(payload) > _MAX_PAYLOAD_BYTES:
            raise MqttMessageError("payload is too large")
        try:
            payload = payload.decode("utf-8")
        except UnicodeDecodeError as error:
            raise MqttMessageError("payload must be UTF-8") from error
    if not isinstance(payload, str):
        raise MqttMessageError("payload must be JSON text")
    try:
        return json.loads(payload, parse_constant=_reject_non_finite_constant)
    except (json.JSONDecodeError, ValueError) as error:
        raise MqttMessageError(f"invalid JSON: {error}") from error


def _reject_non_finite_constant(value: str) -> None:
    raise ValueError(f"non-finite JSON constant {value}")


def _required_uint(payload: dict[str, Any], field_name: str, maximum: int) -> int:
    if field_name not in payload:
        raise MqttMessageError(f"missing required field: {field_name}")
    value = payload[field_name]
    if isinstance(value, bool) or not isinstance(value, int) or not 0 <= value <= maximum:
        raise MqttMessageError(f"{field_name} must be an unsigned integer")
    return value


def _measurement(payload: dict[str, Any], field_name: str, minimum: float, maximum: float) -> float | None:
    value = payload.get(field_name)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise MqttMessageError(f"{field_name} must be a number or null")
    value = float(value)
    if not math.isfinite(value) or not minimum <= value <= maximum:
        raise MqttMessageError(f"{field_name} is outside the accepted range")
    return value


def _optional_bool(payload: dict[str, Any], field_name: str) -> bool | None:
    value = payload.get(field_name)
    if value is None:
        return None
    if not isinstance(value, bool):
        raise MqttMessageError(f"{field_name} must be a boolean or null")
    return value
