from __future__ import annotations

from datetime import datetime
from decimal import Decimal

import pytest

from beehive_gateway.config import Settings
from beehive_gateway.protocol import SensorReading
from beehive_gateway.storage import MySqlStorage, open_mysql_storage
from beehive_gateway.telemetry_state import MinuteAggregate


class FakeCursor:
    def __init__(self, rowcount: int = 1, error: Exception | None = None) -> None:
        self.rowcount = rowcount
        self.error = error
        self.executed: tuple[str, tuple[object, ...]] | None = None
        self.executed_all: list[tuple[str, tuple[object, ...]]] = []
        self.closed = False

    def execute(self, statement: str, parameters: tuple[object, ...]) -> None:
        if self.error is not None:
            raise self.error
        self.executed = statement, parameters
        self.executed_all.append((statement, parameters))

    def close(self) -> None:
        self.closed = True


class FakeConnection:
    def __init__(self, cursor: FakeCursor) -> None:
        self.cursor_instance = cursor
        self.commit_count = 0
        self.rollback_count = 0
        self.closed = False

    def cursor(self) -> FakeCursor:
        return self.cursor_instance

    def commit(self) -> None:
        self.commit_count += 1

    def rollback(self) -> None:
        self.rollback_count += 1

    def close(self) -> None:
        self.closed = True


class FakeConnector:
    def __init__(self, connection: FakeConnection) -> None:
        self.connection = connection
        self.connect_kwargs: dict[str, object] | None = None

    def connect(self, **kwargs: object) -> FakeConnection:
        self.connect_kwargs = kwargs
        return self.connection


@pytest.fixture
def null_reading() -> SensorReading:
    return SensorReading(
        node_id=1,
        sequence_no=12,
        uptime_ms=25000,
        inside_temperature=None,
        inside_humidity=None,
        outside_temperature=24.75,
        outside_humidity=56.78,
        tilted=None,
        status_flags=2,
    )


def test_inserts_measurement_and_preserves_nulls(null_reading: SensorReading) -> None:
    cursor = FakeCursor(rowcount=1)
    connection = FakeConnection(cursor)

    inserted = MySqlStorage(connection).store(null_reading)

    assert inserted is True
    assert connection.commit_count == 1
    assert cursor.closed is True
    assert cursor.executed is not None
    statement, parameters = cursor.executed
    assert "ON DUPLICATE KEY UPDATE" in statement
    assert parameters == (1, 12, 25000, None, None, 24.75, 56.78, None, 2)


def test_duplicate_node_sequence_is_not_inserted(null_reading: SensorReading) -> None:
    connection = FakeConnection(FakeCursor(rowcount=0))

    inserted = MySqlStorage(connection).store(null_reading)

    assert inserted is False
    assert connection.commit_count == 1


def test_mqtt_reading_updates_gateway_last_seen(null_reading: SensorReading) -> None:
    cursor = FakeCursor(rowcount=1)
    connection = FakeConnection(cursor)

    inserted = MySqlStorage(connection).store_gateway_reading("gw-001", null_reading)

    assert inserted is True
    assert connection.commit_count == 1
    assert len(cursor.executed_all) == 2
    assert "INSERT INTO sensor_readings" in cursor.executed_all[0][0]
    assert cursor.executed_all[0][1][0] == "gw-001"
    assert "INSERT INTO gateways" in cursor.executed_all[1][0]
    assert cursor.executed_all[1][1] == ("gw-001", "online")


def test_database_write_failure_rolls_back(null_reading: SensorReading) -> None:
    connection = FakeConnection(FakeCursor(error=RuntimeError("database unavailable")))

    with pytest.raises(RuntimeError, match="database unavailable"):
        MySqlStorage(connection).store(null_reading)

    assert connection.rollback_count == 1


def test_close_releases_database_connection() -> None:
    connection = FakeConnection(FakeCursor())

    MySqlStorage(connection).close()

    assert connection.closed is True


def test_stores_one_minute_aggregate() -> None:
    cursor = FakeCursor(rowcount=1)
    connection = FakeConnection(cursor)
    aggregate = MinuteAggregate(
        gateway_id="gw-001", node_id=1, bucket_start=datetime(2026, 8, 16, 12, 0), sample_count=30,
        inside_temperature_avg=Decimal("25.50"), inside_temperature_min=Decimal("25.00"), inside_temperature_max=Decimal("26.00"), inside_temperature_valid_count=30,
        inside_humidity_avg=Decimal("50.00"), inside_humidity_min=Decimal("49.00"), inside_humidity_max=Decimal("51.00"), inside_humidity_valid_count=30,
        outside_temperature_avg=None, outside_temperature_min=None, outside_temperature_max=None, outside_temperature_valid_count=0,
        outside_humidity_avg=None, outside_humidity_min=None, outside_humidity_max=None, outside_humidity_valid_count=0,
        tilt_valid_count=30, tilt_alarm_count=1, status_flags_or=15,
        last_sequence_no=30, last_uptime_ms=60000, last_received_at=datetime(2026, 8, 16, 12, 0, 58),
    )

    MySqlStorage(connection).store_minute_aggregate(aggregate)

    assert connection.commit_count == 1
    assert cursor.executed is not None
    statement, parameters = cursor.executed
    assert "INSERT INTO sensor_minute_aggregates" in statement
    assert parameters[:4] == ("gw-001", 1, datetime(2026, 8, 16, 12, 0), 30)


def test_opens_mysql_connection_from_settings() -> None:
    connection = FakeConnection(FakeCursor())
    connector = FakeConnector(connection)
    settings = Settings(
        serial_port="COM7",
        serial_baud=115200,
        database_host="127.0.0.1",
        database_port=3306,
        database_name="beehive_gateway",
        database_user="beehive_app",
        database_password="not-a-real-password",
    )

    storage = open_mysql_storage(settings, connector=connector)

    assert isinstance(storage, MySqlStorage)
    assert connector.connect_kwargs == {
        "host": "127.0.0.1",
        "port": 3306,
        "database": "beehive_gateway",
        "user": "beehive_app",
        "password": "not-a-real-password",
        "autocommit": False,
    }


class FakeDictCursor(FakeCursor):
    """Cursor that answers queries from a scripted result map."""

    def __init__(self, results: dict[str, list[dict[str, object]]]) -> None:
        super().__init__()
        self.results = results

    def execute(self, statement: str, parameters: tuple[object, ...]) -> None:
        super().execute(statement, parameters)
        self._rows = self.results.get(_result_key(statement), [])

    def fetchall(self) -> list[dict[str, object]]:
        return self._rows

    @property
    def description(self) -> list[tuple[str, ...]]:
        return []


def _result_key(statement: str) -> str:
    if "SUM(sample_count)" in statement:
        return "quality"
    if "COUNT(*)" in statement:
        return "count"
    return "export"


def test_minute_export_rows_orders_ascending_and_filters_time_range() -> None:
    cursor = FakeDictCursor({"export": [{"gateway_id": "gw-001", "bucket_start": "t1"}]})
    storage = MySqlStorage(FakeConnection(cursor))

    rows = storage.minute_export_rows(
        "gw-001",
        1,
        start=datetime(2026, 9, 1),
        end=datetime(2026, 9, 2),
    )

    assert rows == [{"gateway_id": "gw-001", "bucket_start": "t1"}]
    statement, parameters = cursor.executed_all[0]
    assert "ORDER BY bucket_start ASC" in statement
    assert "bucket_start >= %s" in statement
    assert "bucket_start <= %s" in statement
    assert parameters[:2] == ("gw-001", 1)


def test_minute_export_count_returns_total() -> None:
    cursor = FakeDictCursor({"count": [{"total": 123}]})

    total = MySqlStorage(FakeConnection(cursor)).minute_export_count("gw-001", 1)

    assert total == 123


def test_minute_quality_summary_computes_missing_and_valid_ratios() -> None:
    cursor = FakeDictCursor({
        "quality": [{
            "minute_rows": 60,
            "first_bucket_start": datetime(2026, 9, 8, 12, 0),
            "last_bucket_start": datetime(2026, 9, 8, 13, 0),
            "total_samples": 1800,
            "inside_temperature_valid_samples": 1800,
            "inside_humidity_valid_samples": 900,
            "outside_temperature_valid_samples": None,
            "outside_humidity_valid_samples": 0,
            "tilt_valid_samples": 1800,
            "tilt_alarm_samples": 5,
        }],
    })

    summary = MySqlStorage(FakeConnection(cursor)).minute_quality_summary("gw-001", 1)

    assert summary["minute_rows"] == 60
    assert summary["expected_minute_rows"] == 61
    assert summary["missing_minute_rows"] == 1
    assert summary["missing_minute_ratio"] == round(1 / 61, 4)
    assert summary["inside_temperature_valid_ratio"] == 1.0
    assert summary["inside_humidity_valid_ratio"] == 0.5
    assert summary["outside_temperature_valid_ratio"] == 0.0
    assert summary["outside_humidity_valid_ratio"] == 0.0
    assert summary["tilt_alarm_ratio"] == round(5 / 1800, 4)


def test_minute_quality_summary_handles_empty_range() -> None:
    cursor = FakeDictCursor({"quality": []})

    summary = MySqlStorage(FakeConnection(cursor)).minute_quality_summary("gw-001", 1)

    assert summary["minute_rows"] == 0
    assert "missing_minute_rows" not in summary
    assert "inside_temperature_valid_ratio" not in summary
