"""Bounded in-memory latest telemetry and per-minute aggregation state."""
from __future__ import annotations

import threading
from collections import deque
from dataclasses import dataclass
from datetime import datetime
from decimal import Decimal, ROUND_HALF_UP

from beehive_gateway.protocol import SensorReading


_METRIC_FIELDS = (
    "inside_temperature",
    "inside_humidity",
    "outside_temperature",
    "outside_humidity",
)
_DEDUP_WINDOW = 256
_DEFAULT_MAX_NODES = 256
_PENDING_AGGREGATE_LIMIT = 100


class TelemetryStateCapacityError(RuntimeError):
    """Raised when untrusted MQTT input exceeds the bounded node cache."""


@dataclass(frozen=True)
class TelemetrySnapshot:
    gateway_id: str
    reading: SensorReading
    received_at: datetime


@dataclass(frozen=True)
class MinuteAggregate:
    gateway_id: str
    node_id: int
    bucket_start: datetime
    sample_count: int
    inside_temperature_avg: Decimal | None
    inside_temperature_min: Decimal | None
    inside_temperature_max: Decimal | None
    inside_temperature_valid_count: int
    inside_humidity_avg: Decimal | None
    inside_humidity_min: Decimal | None
    inside_humidity_max: Decimal | None
    inside_humidity_valid_count: int
    outside_temperature_avg: Decimal | None
    outside_temperature_min: Decimal | None
    outside_temperature_max: Decimal | None
    outside_temperature_valid_count: int
    outside_humidity_avg: Decimal | None
    outside_humidity_min: Decimal | None
    outside_humidity_max: Decimal | None
    outside_humidity_valid_count: int
    tilt_valid_count: int
    tilt_alarm_count: int
    status_flags_or: int
    last_sequence_no: int
    last_uptime_ms: int
    last_received_at: datetime


@dataclass
class _MetricStats:
    total: Decimal = Decimal("0")
    count: int = 0
    minimum: Decimal | None = None
    maximum: Decimal | None = None

    def add(self, value: float | None) -> None:
        if value is None:
            return
        decimal_value = Decimal(str(value))
        self.total += decimal_value
        self.count += 1
        self.minimum = decimal_value if self.minimum is None else min(self.minimum, decimal_value)
        self.maximum = decimal_value if self.maximum is None else max(self.maximum, decimal_value)

    def average(self) -> Decimal | None:
        if self.count == 0:
            return None
        return (self.total / self.count).quantize(Decimal("0.01"), rounding=ROUND_HALF_UP)


@dataclass
class _MutableMinuteAggregate:
    gateway_id: str
    node_id: int
    bucket_start: datetime
    sample_count: int = 0
    tilt_valid_count: int = 0
    tilt_alarm_count: int = 0
    status_flags_or: int = 0
    last_sequence_no: int = 0
    last_uptime_ms: int = 0
    last_received_at: datetime | None = None

    def __post_init__(self) -> None:
        self.metrics = {name: _MetricStats() for name in _METRIC_FIELDS}

    def add(self, reading: SensorReading, received_at: datetime) -> None:
        self.sample_count += 1
        for field_name, stats in self.metrics.items():
            stats.add(getattr(reading, field_name))
        if reading.tilted is not None:
            self.tilt_valid_count += 1
        if reading.tilted is True or (reading.status_flags & 0x0C) == 0x0C:
            self.tilt_alarm_count += 1
        self.status_flags_or |= reading.status_flags
        self.last_sequence_no = reading.sequence_no
        self.last_uptime_ms = reading.uptime_ms
        self.last_received_at = received_at

    def freeze(self) -> MinuteAggregate:
        if self.last_received_at is None:
            raise RuntimeError("cannot persist an empty minute aggregate")
        values: dict[str, Decimal | int | None] = {}
        for name, stats in self.metrics.items():
            values[f"{name}_avg"] = stats.average()
            values[f"{name}_min"] = stats.minimum
            values[f"{name}_max"] = stats.maximum
            values[f"{name}_valid_count"] = stats.count
        return MinuteAggregate(
            gateway_id=self.gateway_id,
            node_id=self.node_id,
            bucket_start=self.bucket_start,
            sample_count=self.sample_count,
            tilt_valid_count=self.tilt_valid_count,
            tilt_alarm_count=self.tilt_alarm_count,
            status_flags_or=self.status_flags_or,
            last_sequence_no=self.last_sequence_no,
            last_uptime_ms=self.last_uptime_ms,
            last_received_at=self.last_received_at,
            **values,
        )


class TelemetryState:
    """Holds one latest reading and one active minute bucket per gateway/node."""

    def __init__(self, max_nodes: int = _DEFAULT_MAX_NODES) -> None:
        if max_nodes <= 0:
            raise ValueError("max_nodes must be positive")
        self._max_nodes = max_nodes
        self._lock = threading.RLock()
        self._latest: dict[tuple[str, int], TelemetrySnapshot] = {}
        self._buckets: dict[tuple[str, int], _MutableMinuteAggregate] = {}
        self._recent_sequences: dict[tuple[str, int], deque[tuple[int, int]]] = {}
        self._pending: deque[MinuteAggregate] = deque(maxlen=_PENDING_AGGREGATE_LIMIT)

    def ingest(self, gateway_id: str, reading: SensorReading, received_at: datetime) -> MinuteAggregate | None:
        """Cache one valid reading and return the prior bucket when a minute rolls over."""
        key = gateway_id, reading.node_id
        marker = reading.sequence_no, reading.uptime_ms
        bucket_start = received_at.replace(second=0, microsecond=0)
        with self._lock:
            if key not in self._latest and len(self._latest) >= self._max_nodes:
                raise TelemetryStateCapacityError(f"node cache capacity {self._max_nodes} reached")
            recent = self._recent_sequences.setdefault(key, deque(maxlen=_DEDUP_WINDOW))
            if marker in recent:
                return None
            recent.append(marker)

            flushed: MinuteAggregate | None = None
            bucket = self._buckets.get(key)
            if bucket is not None and bucket.bucket_start != bucket_start:
                flushed = bucket.freeze()
                bucket = None
            if bucket is None:
                bucket = _MutableMinuteAggregate(gateway_id, reading.node_id, bucket_start)
                self._buckets[key] = bucket

            bucket.add(reading, received_at)
            self._latest[key] = TelemetrySnapshot(gateway_id, reading, received_at)
            return flushed

    def latest(self, gateway_id: str, node_id: int) -> TelemetrySnapshot | None:
        with self._lock:
            return self._latest.get((gateway_id, node_id))

    def flush(self) -> list[MinuteAggregate]:
        """Return and clear all non-empty minute buckets, used during orderly shutdown."""
        with self._lock:
            aggregates = [bucket.freeze() for bucket in self._buckets.values()]
            self._buckets.clear()
            return aggregates

    def flush_completed(self, now: datetime) -> list[MinuteAggregate]:
        """Return buckets from earlier minutes even when the node has gone silent."""
        current_bucket = now.replace(second=0, microsecond=0)
        with self._lock:
            completed_keys = [key for key, bucket in self._buckets.items() if bucket.bucket_start < current_bucket]
            aggregates = [self._buckets.pop(key).freeze() for key in completed_keys]
            return aggregates

    def node_ids(self, gateway_id: str) -> set[int]:
        with self._lock:
            return {node_id for cached_gateway_id, node_id in self._latest if cached_gateway_id == gateway_id}

    def queue_pending(self, aggregate: MinuteAggregate) -> None:
        with self._lock:
            if len(self._pending) >= _PENDING_AGGREGATE_LIMIT:
                self._pending.popleft()
            self._pending.append(aggregate)

    def pending(self) -> tuple[MinuteAggregate, ...]:
        with self._lock:
            return tuple(self._pending)

    def acknowledge_pending(self, aggregate: MinuteAggregate) -> None:
        with self._lock:
            if self._pending and self._pending[0] == aggregate:
                self._pending.popleft()
