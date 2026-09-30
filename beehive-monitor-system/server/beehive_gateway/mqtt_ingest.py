"""MQTT worker for validated gateway telemetry and status messages."""

from __future__ import annotations

import logging
import queue
import threading
from dataclasses import dataclass
from datetime import datetime
from typing import Any, Protocol
from urllib.parse import urlparse

from beehive_gateway.config import ConfigurationError, Settings, load_web_settings
from beehive_gateway.mqtt_protocol import (
    MqttMessageError,
    parse_status_payload,
    parse_flat_status_topic,
    parse_flat_telemetry_topic,
    parse_status_topic,
    parse_telemetry_payload,
    parse_telemetry_topic,
)
from beehive_gateway.storage import open_mysql_storage
from beehive_gateway.telemetry_state import MinuteAggregate, TelemetryState


LOGGER = logging.getLogger(__name__)


class MqttStorage(Protocol):
    def store_gateway_reading(self, gateway_id: str, reading: object) -> bool: ...

    def update_gateway_status(self, gateway_id: str, status: str) -> None: ...

    def store_minute_aggregate(self, aggregate: MinuteAggregate) -> None: ...


def process_mqtt_message(
    topic: str,
    payload: bytes,
    storage: MqttStorage,
    topic_prefix: str = "beehive/v1",
    *,
    state: TelemetryState | None = None,
    received_at: datetime | None = None,
    topic_mode: str = "hierarchical",
    flat_telemetry_topic: str = "",
    flat_status_topic: str = "",
    flat_gateway_id: str = "gw-001",
    flat_node_id: int = 1,
) -> str:
    """Validate and persist one message; malformed input never escapes this boundary."""
    try:
        if topic_mode == "bemfa":
            if topic == flat_status_topic:
                gateway_id = parse_flat_status_topic(topic, flat_status_topic, flat_gateway_id)
            else:
                address = parse_flat_telemetry_topic(topic, flat_telemetry_topic, flat_gateway_id, flat_node_id)
                message = parse_telemetry_payload(payload, address)
                if state is not None:
                    aggregate = state.ingest(message.gateway_id, message.reading, received_at or datetime.now())
                    if aggregate is not None:
                        try:
                            storage.store_minute_aggregate(aggregate)
                        except Exception:
                            state.queue_pending(aggregate)
                            raise
                        return "aggregated"
                    return "cached"
                inserted = storage.store_gateway_reading(message.gateway_id, message.reading)
                return "inserted" if inserted else "duplicate"
            status = parse_status_payload(payload)
            storage.update_gateway_status(gateway_id, status)
            return "status"

        if topic.endswith("/status"):
            gateway_id = parse_status_topic(topic, topic_prefix)
            status = parse_status_payload(payload)
            storage.update_gateway_status(gateway_id, status)
            LOGGER.info("Updated gateway status gateway_id=%s status=%s", gateway_id, status)
            return "status"

        address = parse_telemetry_topic(topic, topic_prefix)
        message = parse_telemetry_payload(payload, address)
        if state is not None:
            aggregate = state.ingest(message.gateway_id, message.reading, received_at or datetime.now())
            if aggregate is not None:
                try:
                    storage.store_minute_aggregate(aggregate)
                except Exception:
                    state.queue_pending(aggregate)
                    raise
                result = "aggregated"
            else:
                result = "cached"
            LOGGER.info(
                "MQTT telemetry %s gateway_id=%s node_id=%d sequence=%d",
                result,
                message.gateway_id,
                message.reading.node_id,
                message.reading.sequence_no,
            )
            return result
        inserted = storage.store_gateway_reading(message.gateway_id, message.reading)
        result = "inserted" if inserted else "duplicate"
        LOGGER.info(
            "MQTT telemetry %s gateway_id=%s node_id=%d sequence=%d",
            result,
            message.gateway_id,
            message.reading.node_id,
            message.reading.sequence_no,
        )
        return result
    except MqttMessageError as error:
        LOGGER.warning("Rejected MQTT message topic=%s reason=%s", topic, error)
        return "invalid"
    except Exception:
        LOGGER.exception("MQTT storage failure topic=%s", topic)
        return "storage_error"


@dataclass(frozen=True)
class _QueuedMessage:
    topic: str
    payload: bytes


class MqttReceiver:
    """Non-blocking Paho MQTT client with a bounded persistence queue."""

    def __init__(self, settings: Settings, storage: MqttStorage, client_factory: Any | None = None, state: TelemetryState | None = None) -> None:
        self._settings = settings
        self._storage = storage
        self._client_factory = client_factory
        self._state = state or TelemetryState()
        self._client: Any | None = None
        self._queue: queue.Queue[_QueuedMessage | None] = queue.Queue(maxsize=100)
        self._worker: threading.Thread | None = None
        self._stopping = threading.Event()

    def start(self) -> None:
        if self._worker is not None:
            return
        self._stopping.clear()
        self._worker = threading.Thread(target=self._worker_loop, name="mqtt-storage", daemon=True)
        self._worker.start()
        try:
            client = self._make_client()
            self._client = client
            client.on_connect = self._on_connect
            client.on_disconnect = self._on_disconnect
            client.on_message = self._on_message
            client.reconnect_delay_set(
                min_delay=max(1, int(self._settings.mqtt_reconnect_delay)),
                max_delay=60,
            )
            if self._settings.mqtt_username:
                client.username_pw_set(self._settings.mqtt_username, self._settings.mqtt_password)
            host, port, uses_tls = parse_mqtt_uri(self._settings.mqtt_uri)
            if uses_tls:
                client.tls_set()
            client.connect_async(
                host,
                port=port,
                keepalive=self._settings.mqtt_keepalive,
            )
            client.loop_start()
            LOGGER.info("MQTT receiver started uri=%s", self._settings.mqtt_uri)
        except Exception:
            LOGGER.exception("MQTT receiver could not start; HTTP service remains available")

    def stop(self) -> None:
        self._stopping.set()
        try:
            self._queue.put_nowait(None)
        except queue.Full:
            pass
        if self._client is not None:
            try:
                self._client.disconnect()
                self._client.loop_stop()
            except Exception:
                LOGGER.exception("Error while stopping MQTT client")
        if self._worker is not None:
            self._worker.join(timeout=2)
        self._persist_completed(datetime.now())
        for aggregate in self._state.flush():
            try:
                self._storage.store_minute_aggregate(aggregate)
            except Exception:
                self._state.queue_pending(aggregate)
                LOGGER.exception("Could not flush minute aggregate during shutdown")
        self._persist_pending()
        self._worker = None

    def _make_client(self) -> Any:
        if self._client_factory is not None:
            return self._client_factory()
        import paho.mqtt.client as mqtt

        try:
            return mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=self._settings.mqtt_client_id)
        except AttributeError:
            return mqtt.Client(client_id=self._settings.mqtt_client_id)

    @property
    def telemetry_state(self) -> TelemetryState:
        return self._state

    def _on_connect(self, client: Any, _userdata: Any, _flags: Any, reason_code: Any, _properties: Any = None) -> None:
        if _reason_failed(reason_code):
            LOGGER.error("MQTT connection rejected reason=%s", reason_code)
            return
        if self._settings.mqtt_topic_mode == "bemfa":
            telemetry_filter = self._settings.mqtt_telemetry_topic
            status_filter = self._settings.mqtt_status_topic
            if not telemetry_filter or not status_filter:
                LOGGER.error("Bemfa MQTT mode requires telemetry and status topics")
                return
        else:
            telemetry_filter = f"{self._settings.mqtt_topic_prefix}/gateways/+/nodes/+/telemetry"
            status_filter = f"{self._settings.mqtt_topic_prefix}/gateways/+/status"
        try:
            client.subscribe([(telemetry_filter, 1), (status_filter, 1)])
            LOGGER.info("MQTT connected and subscribed")
        except Exception:
            LOGGER.exception("MQTT subscription failed; reconnect will continue")

    def _on_disconnect(self, _client: Any, _userdata: Any, _disconnect_flags: Any = None, reason_code: Any = None, _properties: Any = None) -> None:
        LOGGER.warning("MQTT disconnected reason=%s", reason_code)

    def _on_message(self, _client: Any, _userdata: Any, message: Any) -> None:
        try:
            self._queue.put_nowait(_QueuedMessage(message.topic, bytes(message.payload)))
        except queue.Full:
            LOGGER.error("MQTT persistence queue is full; dropping topic=%s", message.topic)

    def _worker_loop(self) -> None:
        while not self._stopping.is_set():
            try:
                item = self._queue.get(timeout=0.5)
            except queue.Empty:
                self._persist_completed(datetime.now())
                continue
            if item is None:
                return
            try:
                self._persist_completed(datetime.now())
                process_mqtt_message(
                    item.topic,
                    item.payload,
                    self._storage,
                    self._settings.mqtt_topic_prefix,
                    state=self._state,
                    topic_mode=self._settings.mqtt_topic_mode,
                    flat_telemetry_topic=self._settings.mqtt_telemetry_topic,
                    flat_status_topic=self._settings.mqtt_status_topic,
                    flat_gateway_id=self._settings.mqtt_gateway_id,
                    flat_node_id=self._settings.mqtt_flat_node_id,
                )
            finally:
                self._queue.task_done()

    def _persist_completed(self, now: datetime) -> None:
        for aggregate in self._state.flush_completed(now):
            try:
                self._storage.store_minute_aggregate(aggregate)
            except Exception:
                self._state.queue_pending(aggregate)
                LOGGER.exception("Could not persist completed minute aggregate")
        self._persist_pending()

    def _persist_pending(self) -> None:
        for aggregate in self._state.pending():
            try:
                self._storage.store_minute_aggregate(aggregate)
            except Exception:
                LOGGER.exception("Could not retry pending minute aggregate")
                return
            self._state.acknowledge_pending(aggregate)


def _reason_failed(reason_code: Any) -> bool:
    failure = getattr(reason_code, "is_failure", None)
    if failure is not None:
        return bool(failure)
    try:
        return int(reason_code) != 0
    except (TypeError, ValueError):
        return False


def parse_mqtt_uri(uri: str) -> tuple[str, int, bool]:
    """Convert a configured MQTT URI into Paho's host/port/TLS arguments."""
    parsed = urlparse(uri)
    if parsed.scheme not in {"mqtt", "mqtts"} or not parsed.hostname:
        raise ValueError("mqtt URI must use mqtt:// or mqtts:// and include a host")
    try:
        port = parsed.port
    except ValueError as error:
        raise ValueError("mqtt URI contains an invalid port") from error
    uses_tls = parsed.scheme == "mqtts"
    return parsed.hostname, port or (8883 if uses_tls else 1883), uses_tls


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(name)s: %(message)s")
    try:
        settings = load_web_settings()
        storage = open_mysql_storage(settings)
    except ConfigurationError as error:
        LOGGER.error("Invalid configuration: %s", error)
        return 2
    except Exception:
        LOGGER.exception("Could not open MySQL database connection")
        return 1

    receiver = MqttReceiver(settings, storage)
    receiver.start()
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        LOGGER.info("MQTT ingestion stopped by user")
    finally:
        receiver.stop()
        storage.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
