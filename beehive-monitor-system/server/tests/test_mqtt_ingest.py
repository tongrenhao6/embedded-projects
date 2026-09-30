from __future__ import annotations

from datetime import datetime

import pytest

from beehive_gateway.config import Settings
from beehive_gateway.mqtt_ingest import MqttReceiver, parse_mqtt_uri, process_mqtt_message
from beehive_gateway.telemetry_state import TelemetryState


class RecordingStorage:
    def __init__(self) -> None:
        self.readings: list[tuple[str, object]] = []
        self.statuses: list[tuple[str, str]] = []
        self.aggregates: list[object] = []

    def store_gateway_reading(self, gateway_id: str, reading: object) -> bool:
        self.readings.append((gateway_id, reading))
        return len(self.readings) == 1

    def update_gateway_status(self, gateway_id: str, status: str) -> None:
        self.statuses.append((gateway_id, status))

    def store_minute_aggregate(self, aggregate: object) -> None:
        self.aggregates.append(aggregate)


def test_processes_valid_telemetry_and_duplicate() -> None:
    storage = RecordingStorage()
    topic = "beehive/v1/gateways/gw-001/nodes/1/telemetry"
    payload = b'{"schema_version":1,"gateway_id":"gw-001","node_id":1,"sequence":1,"uptime_ms":2,"inside_temperature":25.0,"inside_humidity":50.0,"outside_temperature":24.0,"outside_humidity":51.0,"tilted":false,"status_flags":7}'

    assert process_mqtt_message(topic, payload, storage) == "inserted"
    assert process_mqtt_message(topic, payload, storage) == "duplicate"


def test_cached_telemetry_is_not_written_as_raw_reading() -> None:
    storage = RecordingStorage()
    state = TelemetryState()
    topic = "beehive/v1/gateways/gw-001/nodes/1/telemetry"
    payload = b'{"schema_version":1,"gateway_id":"gw-001","node_id":1,"sequence":1,"uptime_ms":2,"inside_temperature":25.0,"inside_humidity":50.0,"outside_temperature":24.0,"outside_humidity":51.0,"tilted":false,"status_flags":7}'

    result = process_mqtt_message(topic, payload, storage, state=state, received_at=datetime(2026, 8, 16, 12, 0, 1))

    assert result == "cached"
    assert storage.readings == []
    assert storage.aggregates == []
    assert state.latest("gw-001", 1) is not None


def test_processes_gateway_status_without_affecting_http_storage() -> None:
    storage = RecordingStorage()

    assert process_mqtt_message("beehive/v1/gateways/gw-001/status", b"online", storage) == "status"
    assert storage.statuses == [("gw-001", "online")]


def test_processes_bemfa_flat_topics() -> None:
    storage = RecordingStorage()
    telemetry_topic = "beehivegw001node001telemetry"
    status_topic = "beehivegw001status"
    payload = b'{"schema_version":1,"gateway_id":"gw-001","node_id":1,"sequence":3,"uptime_ms":4,"inside_temperature":25.0,"inside_humidity":50.0,"outside_temperature":24.0,"outside_humidity":51.0,"tilted":false,"status_flags":7}'

    assert process_mqtt_message(
        telemetry_topic, payload, storage, topic_mode="bemfa",
        flat_telemetry_topic=telemetry_topic,
        flat_status_topic=status_topic,
        flat_gateway_id="gw-001", flat_node_id=1,
    ) == "inserted"
    assert process_mqtt_message(
        status_topic, b"online", storage, topic_mode="bemfa",
        flat_telemetry_topic=telemetry_topic,
        flat_status_topic=status_topic,
        flat_gateway_id="gw-001", flat_node_id=1,
    ) == "status"
    assert storage.statuses == [("gw-001", "online")]


def test_parses_mqtt_uri_for_paho_connection() -> None:
    assert parse_mqtt_uri("mqtt://broker.emqx.io:1883") == ("broker.emqx.io", 1883, False)
    assert parse_mqtt_uri("mqtts://example.test") == ("example.test", 8883, True)


def test_rejects_unsupported_mqtt_uri() -> None:
    with pytest.raises(ValueError, match="mqtt URI"):
        parse_mqtt_uri("http://example.test")


def test_web_receiver_subscribes_without_publishing_gateway_status() -> None:
    class FakeClient:
        def __init__(self) -> None:
            self.subscriptions = []
            self.publishes = []

        def subscribe(self, topics):
            self.subscriptions.append(topics)

        def publish(self, *args, **kwargs):
            self.publishes.append((args, kwargs))

    settings = Settings(
        serial_port="",
        serial_baud=115200,
        database_host="127.0.0.1",
        database_port=3306,
        database_name="beehive_gateway",
        database_user="beehive_app",
        database_password="not-a-real-password",
    )
    client = FakeClient()
    receiver = MqttReceiver(settings, RecordingStorage())

    receiver._on_connect(client, None, None, 0)

    assert client.subscriptions == [[
        ("beehive/v1/gateways/+/nodes/+/telemetry", 1),
        ("beehive/v1/gateways/+/status", 1),
    ]]
    assert client.publishes == []


def test_web_receiver_subscribes_to_bemfa_topics() -> None:
    class FakeClient:
        def __init__(self) -> None:
            self.subscriptions = []

        def subscribe(self, topics):
            self.subscriptions.append(topics)

    settings = Settings(
        serial_port="", serial_baud=115200, database_host="127.0.0.1", database_port=3306,
        database_name="beehive_gateway", database_user="beehive_app", database_password="not-a-real-password",
        mqtt_topic_mode="bemfa", mqtt_telemetry_topic="beehivegw001node001telemetry",
        mqtt_status_topic="beehivegw001status", mqtt_gateway_id="gw-001", mqtt_flat_node_id=1,
    )
    client = FakeClient()
    MqttReceiver(settings, RecordingStorage())._on_connect(client, None, None, 0)
    assert client.subscriptions == [[
        ("beehivegw001node001telemetry", 1), ("beehivegw001status", 1),
    ]]


def test_receiver_configures_connection_before_starting_network_loop() -> None:
    class FakeClient:
        def __init__(self) -> None:
            self.calls = []

        def reconnect_delay_set(self, **kwargs):
            self.calls.append("reconnect_delay_set")

        def connect_async(self, host, port, keepalive):
            self.calls.append(("connect_async", host, port, keepalive))

        def loop_start(self):
            self.calls.append("loop_start")

        def disconnect(self):
            self.calls.append("disconnect")

        def loop_stop(self):
            self.calls.append("loop_stop")

    settings = Settings(
        serial_port="",
        serial_baud=115200,
        database_host="127.0.0.1",
        database_port=3306,
        database_name="beehive_gateway",
        database_user="beehive_app",
        database_password="not-a-real-password",
    )
    client = FakeClient()
    receiver = MqttReceiver(settings, RecordingStorage(), client_factory=lambda: client)

    receiver.start()
    receiver.stop()

    assert client.calls.index(("connect_async", "broker.emqx.io", 1883, 30)) < client.calls.index("loop_start")
