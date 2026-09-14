"""Runtime configuration loaded from environment variables."""

from __future__ import annotations

import os
from dataclasses import dataclass
from typing import Mapping


class ConfigurationError(ValueError):
    """Raised when required runtime configuration is missing or invalid."""


@dataclass(frozen=True)
class Settings:
    serial_port: str
    serial_baud: int
    database_host: str
    database_port: int
    database_name: str
    database_user: str
    database_password: str
    mqtt_uri: str = "mqtt://broker.emqx.io:1883"
    mqtt_username: str = ""
    mqtt_password: str = ""
    mqtt_client_id: str = "beehive_web"
    mqtt_topic_prefix: str = "beehive/v1"
    mqtt_keepalive: int = 30
    mqtt_reconnect_delay: float = 3.0
    mqtt_topic_mode: str = "hierarchical"
    mqtt_telemetry_topic: str = ""
    mqtt_status_topic: str = ""
    mqtt_gateway_id: str = "gw-001"
    mqtt_flat_node_id: int = 1
    http_host: str = "127.0.0.1"
    http_port: int = 8000


def load_settings(environment: Mapping[str, str] | None = None) -> Settings:
    """Load settings without exposing credentials in command arguments."""
    values = os.environ if environment is None else environment
    return _load_settings(values, require_serial=True)


def load_web_settings(environment: Mapping[str, str] | None = None) -> Settings:
    """Load settings for the MQTT/HTTP service without requiring a serial port."""
    values = os.environ if environment is None else environment
    return _load_settings(values, require_serial=False)


def _load_settings(values: Mapping[str, str], *, require_serial: bool) -> Settings:
    return Settings(
        serial_port=_required(values, "BEEHIVE_SERIAL_PORT") if require_serial else values.get("BEEHIVE_SERIAL_PORT", ""),
        serial_baud=_positive_int(values.get("BEEHIVE_SERIAL_BAUD", "115200"), "BEEHIVE_SERIAL_BAUD"),
        database_host=values.get("BEEHIVE_DB_HOST", "127.0.0.1"),
        database_port=_positive_int(values.get("BEEHIVE_DB_PORT", "3306"), "BEEHIVE_DB_PORT"),
        database_name=values.get("BEEHIVE_DB_NAME", "beehive_gateway"),
        database_user=values.get("BEEHIVE_DB_USER", "beehive_app"),
        database_password=_required(values, "BEEHIVE_DB_PASSWORD"),
        mqtt_uri=values.get("BEEHIVE_MQTT_URI", "mqtt://broker.emqx.io:1883"),
        mqtt_username=values.get("BEEHIVE_MQTT_USERNAME", ""),
        mqtt_password=values.get("BEEHIVE_MQTT_PASSWORD", ""),
        mqtt_client_id=values.get("BEEHIVE_MQTT_CLIENT_ID", "beehive_web"),
        mqtt_topic_prefix=values.get("BEEHIVE_MQTT_TOPIC_PREFIX", "beehive/v1").strip("/"),
        mqtt_keepalive=_positive_int(values.get("BEEHIVE_MQTT_KEEPALIVE", "30"), "BEEHIVE_MQTT_KEEPALIVE"),
        mqtt_reconnect_delay=_positive_float(values.get("BEEHIVE_MQTT_RECONNECT_DELAY", "3"), "BEEHIVE_MQTT_RECONNECT_DELAY"),
        mqtt_topic_mode=values.get("BEEHIVE_MQTT_TOPIC_MODE", "hierarchical").strip().lower(),
        mqtt_telemetry_topic=values.get("BEEHIVE_MQTT_TELEMETRY_TOPIC", "").strip(),
        mqtt_status_topic=values.get("BEEHIVE_MQTT_STATUS_TOPIC", "").strip(),
        mqtt_gateway_id=values.get("BEEHIVE_MQTT_GATEWAY_ID", "gw-001").strip(),
        mqtt_flat_node_id=_positive_int(values.get("BEEHIVE_MQTT_FLAT_NODE_ID", "1"), "BEEHIVE_MQTT_FLAT_NODE_ID"),
        http_host=values.get("BEEHIVE_HTTP_HOST", "127.0.0.1"),
        http_port=_positive_int(values.get("BEEHIVE_HTTP_PORT", "8000"), "BEEHIVE_HTTP_PORT"),
    )


def _required(values: Mapping[str, str], name: str) -> str:
    value = values.get(name, "").strip()
    if not value:
        raise ConfigurationError(f"{name} must be set")
    return value


def _positive_int(raw_value: str, name: str) -> int:
    try:
        value = int(raw_value)
    except ValueError as error:
        raise ConfigurationError(f"{name} must be a positive integer") from error
    if value <= 0:
        raise ConfigurationError(f"{name} must be a positive integer")
    return value


def _positive_float(raw_value: str, name: str) -> float:
    try:
        value = float(raw_value)
    except ValueError as error:
        raise ConfigurationError(f"{name} must be a positive number") from error
    if value <= 0:
        raise ConfigurationError(f"{name} must be a positive number")
    return value
