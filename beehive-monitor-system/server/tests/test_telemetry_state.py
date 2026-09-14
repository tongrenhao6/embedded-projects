from __future__ import annotations

from datetime import datetime
from decimal import Decimal

from beehive_gateway.protocol import SensorReading
from beehive_gateway.telemetry_state import TelemetryState
from beehive_gateway.telemetry_state import TelemetryStateCapacityError


def reading(
    *,
    sequence_no: int,
    inside_temperature: float | None = 25.0,
    inside_humidity: float | None = 50.0,
    outside_temperature: float | None = 24.0,
    outside_humidity: float | None = 55.0,
    tilted: bool | None = False,
    status_flags: int = 7,
) -> SensorReading:
    return SensorReading(
        node_id=1,
        sequence_no=sequence_no,
        uptime_ms=sequence_no * 2_000,
        inside_temperature=inside_temperature,
        inside_humidity=inside_humidity,
        outside_temperature=outside_temperature,
        outside_humidity=outside_humidity,
        tilted=tilted,
        status_flags=status_flags,
    )


def test_minute_change_flushes_aggregate_and_ignores_null_measurements() -> None:
    state = TelemetryState()
    timestamp = datetime(2026, 8, 16, 12, 0, 1)

    assert state.ingest("gw-001", reading(sequence_no=1, inside_temperature=25.0), timestamp) is None
    assert state.ingest("gw-001", reading(sequence_no=2, inside_temperature=None), timestamp) is None
    aggregate = state.ingest("gw-001", reading(sequence_no=3, inside_temperature=27.0), datetime(2026, 8, 16, 12, 1))

    assert aggregate is not None
    assert aggregate.bucket_start == datetime(2026, 8, 16, 12, 0)
    assert aggregate.sample_count == 2
    assert aggregate.inside_temperature_avg == Decimal("25.00")
    assert aggregate.inside_temperature_min == Decimal("25.00")
    assert aggregate.inside_temperature_max == Decimal("25.00")
    assert aggregate.inside_temperature_valid_count == 1


def test_tilt_alarm_and_latest_cache_are_updated_immediately() -> None:
    state = TelemetryState()
    now = datetime(2026, 8, 16, 12, 0, 1)
    alarm = reading(sequence_no=1, tilted=True, status_flags=15)

    state.ingest("gw-001", alarm, now)
    latest = state.latest("gw-001", 1)
    aggregate = state.flush()[0]

    assert latest is not None
    assert latest.reading == alarm
    assert latest.received_at == now
    assert aggregate.tilt_valid_count == 1
    assert aggregate.tilt_alarm_count == 1
    assert aggregate.status_flags_or == 15


def test_duplicate_sequence_does_not_change_current_minute() -> None:
    state = TelemetryState()
    now = datetime(2026, 8, 16, 12, 0, 1)
    sample = reading(sequence_no=9, inside_temperature=25.0)

    assert state.ingest("gw-001", sample, now) is None
    assert state.ingest("gw-001", sample, now) is None
    aggregate = state.flush()[0]

    assert aggregate.sample_count == 1
    assert aggregate.inside_temperature_valid_count == 1


def test_completed_minute_flushes_without_a_following_telemetry_message() -> None:
    state = TelemetryState()
    state.ingest("gw-001", reading(sequence_no=1), datetime(2026, 8, 16, 12, 0, 1))

    aggregates = state.flush_completed(datetime(2026, 8, 16, 12, 1, 0))

    assert len(aggregates) == 1
    assert aggregates[0].bucket_start == datetime(2026, 8, 16, 12, 0)


def test_capacity_limit_rejects_untrusted_new_nodes() -> None:
    state = TelemetryState(max_nodes=1)
    now = datetime(2026, 8, 16, 12, 0, 1)

    state.ingest("gw-001", reading(sequence_no=1), now)
    another_node = SensorReading(2, 1, 1, 25.0, 50.0, 24.0, 51.0, False, 7)

    with __import__("pytest").raises(TelemetryStateCapacityError):
        state.ingest("gw-001", another_node, now)
