"""Validation for the gateway's line-oriented GWJSON output."""

from __future__ import annotations

import json
from dataclasses import dataclass
from typing import Any


GATEWAY_JSON_PREFIX = "GWJSON "


class GatewayFrameError(ValueError):
    """Raised when a GWJSON line cannot be safely persisted."""


@dataclass(frozen=True)
class SensorReading:
    node_id: int
    sequence_no: int
    uptime_ms: int
    inside_temperature: float | None
    inside_humidity: float | None
    outside_temperature: float | None
    outside_humidity: float | None
    tilted: bool | None
    status_flags: int


def parse_gateway_line(line: str) -> SensorReading | None:
    """Parse one serial line, ignoring regular ESP-IDF logs."""
    if not line.startswith(GATEWAY_JSON_PREFIX):
        return None

    payload_text = line[len(GATEWAY_JSON_PREFIX) :]
    try:
        payload = json.loads(payload_text)
    except json.JSONDecodeError as error:
        raise GatewayFrameError(f"invalid JSON: {error.msg}") from error

    if not isinstance(payload, dict):
        raise GatewayFrameError("GWJSON payload must be an object")

    return SensorReading(
        node_id=_required_uint(payload, "node_id", 0xFFFF),
        sequence_no=_required_uint(payload, "sequence", 0xFFFFFFFF),
        uptime_ms=_required_uint(payload, "uptime_ms", 0xFFFFFFFF),
        inside_temperature=_optional_number(payload, "inside_temperature"),
        inside_humidity=_optional_number(payload, "inside_humidity"),
        outside_temperature=_optional_number(payload, "outside_temperature"),
        outside_humidity=_optional_number(payload, "outside_humidity"),
        tilted=_optional_bool(payload, "tilted"),
        status_flags=_required_uint(payload, "status_flags", 0xFFFF),
    )


def _required_uint(payload: dict[str, Any], field_name: str, maximum: int) -> int:
    if field_name not in payload:
        raise GatewayFrameError(f"missing required field: {field_name}")

    value = payload[field_name]
    if isinstance(value, bool) or not isinstance(value, int):
        raise GatewayFrameError(f"{field_name} must be an unsigned integer")
    if not 0 <= value <= maximum:
        raise GatewayFrameError(f"{field_name} must be between 0 and {maximum}")
    return value


def _optional_number(payload: dict[str, Any], field_name: str) -> float | None:
    value = payload.get(field_name)
    if value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise GatewayFrameError(f"{field_name} must be a number or null")
    return float(value)


def _optional_bool(payload: dict[str, Any], field_name: str) -> bool | None:
    value = payload.get(field_name)
    if value is None:
        return None
    if not isinstance(value, bool):
        raise GatewayFrameError(f"{field_name} must be a boolean or null")
    return value
