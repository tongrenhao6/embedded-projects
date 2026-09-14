"""MySQL persistence for validated sensor readings."""

from __future__ import annotations

import threading
from decimal import Decimal
from typing import Any

from beehive_gateway.config import Settings
from beehive_gateway.protocol import SensorReading
from beehive_gateway.telemetry_state import MinuteAggregate


INSERT_SENSOR_READING_SQL = """
INSERT INTO sensor_readings (
    node_id,
    sequence_no,
    uptime_ms,
    inside_temperature,
    inside_humidity,
    outside_temperature,
    outside_humidity,
    tilted,
    status_flags
) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s)
ON DUPLICATE KEY UPDATE id = id
"""

INSERT_GATEWAY_READING_SQL = """
INSERT INTO sensor_readings (
    gateway_id,
    node_id,
    sequence_no,
    uptime_ms,
    inside_temperature,
    inside_humidity,
    outside_temperature,
    outside_humidity,
    tilted,
    status_flags
) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
ON DUPLICATE KEY UPDATE id = id
"""

UPSERT_GATEWAY_STATUS_SQL = """
INSERT INTO gateways (gateway_id, status, last_seen_at)
VALUES (%s, %s, CURRENT_TIMESTAMP(3))
ON DUPLICATE KEY UPDATE
    status = VALUES(status),
    last_seen_at = VALUES(last_seen_at),
    updated_at = CURRENT_TIMESTAMP(3)
"""

INSERT_MINUTE_AGGREGATE_SQL = """
INSERT INTO sensor_minute_aggregates (
    gateway_id, node_id, bucket_start, sample_count,
    inside_temperature_avg, inside_temperature_min, inside_temperature_max, inside_temperature_valid_count,
    inside_humidity_avg, inside_humidity_min, inside_humidity_max, inside_humidity_valid_count,
    outside_temperature_avg, outside_temperature_min, outside_temperature_max, outside_temperature_valid_count,
    outside_humidity_avg, outside_humidity_min, outside_humidity_max, outside_humidity_valid_count,
    tilt_valid_count, tilt_alarm_count, status_flags_or,
    last_sequence_no, last_uptime_ms, last_received_at
) VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
ON DUPLICATE KEY UPDATE
    sample_count = VALUES(sample_count),
    inside_temperature_avg = VALUES(inside_temperature_avg),
    inside_temperature_min = VALUES(inside_temperature_min),
    inside_temperature_max = VALUES(inside_temperature_max),
    inside_temperature_valid_count = VALUES(inside_temperature_valid_count),
    inside_humidity_avg = VALUES(inside_humidity_avg),
    inside_humidity_min = VALUES(inside_humidity_min),
    inside_humidity_max = VALUES(inside_humidity_max),
    inside_humidity_valid_count = VALUES(inside_humidity_valid_count),
    outside_temperature_avg = VALUES(outside_temperature_avg),
    outside_temperature_min = VALUES(outside_temperature_min),
    outside_temperature_max = VALUES(outside_temperature_max),
    outside_temperature_valid_count = VALUES(outside_temperature_valid_count),
    outside_humidity_avg = VALUES(outside_humidity_avg),
    outside_humidity_min = VALUES(outside_humidity_min),
    outside_humidity_max = VALUES(outside_humidity_max),
    outside_humidity_valid_count = VALUES(outside_humidity_valid_count),
    tilt_valid_count = VALUES(tilt_valid_count),
    tilt_alarm_count = VALUES(tilt_alarm_count),
    status_flags_or = VALUES(status_flags_or),
    last_sequence_no = VALUES(last_sequence_no),
    last_uptime_ms = VALUES(last_uptime_ms),
    last_received_at = VALUES(last_received_at)
"""


class MySqlStorage:
    """Store readings through one MySQL connection."""

    def __init__(self, connection: Any) -> None:
        self._connection = connection
        self._lock = threading.RLock()

    def store(self, reading: SensorReading) -> bool:
        """Insert a reading and return False when its node/sequence already exists."""
        return self._execute_insert(
            INSERT_SENSOR_READING_SQL,
            (
                reading.node_id,
                reading.sequence_no,
                reading.uptime_ms,
                reading.inside_temperature,
                reading.inside_humidity,
                reading.outside_temperature,
                reading.outside_humidity,
                reading.tilted,
                reading.status_flags,
            ),
        )

    def store_gateway_reading(self, gateway_id: str, reading: SensorReading) -> bool:
        """Insert an MQTT reading, deduplicated by gateway/node/sequence."""
        with self._lock:
            cursor = self._connection.cursor()
            try:
                cursor.execute(
                    INSERT_GATEWAY_READING_SQL,
                    (
                        gateway_id,
                        reading.node_id,
                        reading.sequence_no,
                        reading.uptime_ms,
                        reading.inside_temperature,
                        reading.inside_humidity,
                        reading.outside_temperature,
                        reading.outside_humidity,
                        reading.tilted,
                        reading.status_flags,
                    ),
                )
                inserted = cursor.rowcount == 1
                cursor.execute(UPSERT_GATEWAY_STATUS_SQL, (gateway_id, "online"))
                self._connection.commit()
                return inserted
            except Exception:
                self._connection.rollback()
                raise
            finally:
                cursor.close()

    def update_gateway_status(self, gateway_id: str, status: str) -> None:
        with self._lock:
            cursor = self._connection.cursor()
            try:
                cursor.execute(UPSERT_GATEWAY_STATUS_SQL, (gateway_id, status))
                self._connection.commit()
            except Exception:
                self._connection.rollback()
                raise
            finally:
                cursor.close()

    def store_minute_aggregate(self, aggregate: MinuteAggregate) -> None:
        """Persist one completed gateway/node minute bucket."""
        parameters = (
            aggregate.gateway_id, aggregate.node_id, aggregate.bucket_start, aggregate.sample_count,
            aggregate.inside_temperature_avg, aggregate.inside_temperature_min, aggregate.inside_temperature_max, aggregate.inside_temperature_valid_count,
            aggregate.inside_humidity_avg, aggregate.inside_humidity_min, aggregate.inside_humidity_max, aggregate.inside_humidity_valid_count,
            aggregate.outside_temperature_avg, aggregate.outside_temperature_min, aggregate.outside_temperature_max, aggregate.outside_temperature_valid_count,
            aggregate.outside_humidity_avg, aggregate.outside_humidity_min, aggregate.outside_humidity_max, aggregate.outside_humidity_valid_count,
            aggregate.tilt_valid_count, aggregate.tilt_alarm_count, aggregate.status_flags_or,
            aggregate.last_sequence_no, aggregate.last_uptime_ms, aggregate.last_received_at,
        )
        self._execute_insert(INSERT_MINUTE_AGGREGATE_SQL, parameters)

    def list_gateways(self) -> list[dict[str, Any]]:
        return self._query(
            """
            SELECT gateway_id, status, last_seen_at, updated_at
            FROM gateways
            ORDER BY gateway_id
            """,
            (),
        )

    def get_gateway(self, gateway_id: str) -> dict[str, Any] | None:
        rows = self._query(
            """
            SELECT gateway_id, status, last_seen_at, updated_at
            FROM gateways
            WHERE gateway_id = %s
            """,
            (gateway_id,),
        )
        return rows[0] if rows else None

    def list_nodes(self, gateway_id: str) -> list[dict[str, Any]]:
        return self._query(
            """
            SELECT gateway_id, node_id, MAX(received_at) AS last_seen_at
            FROM sensor_readings
            WHERE gateway_id = %s
            GROUP BY gateway_id, node_id
            ORDER BY node_id
            """,
            (gateway_id,),
        )

    def list_minute_nodes(self, gateway_id: str) -> list[dict[str, Any]]:
        return self._query(
            """
            SELECT gateway_id, node_id, MAX(last_received_at) AS last_seen_at
            FROM sensor_minute_aggregates
            WHERE gateway_id = %s
            GROUP BY gateway_id, node_id
            ORDER BY node_id
            """,
            (gateway_id,),
        )

    def latest_reading(self, gateway_id: str, node_id: int) -> dict[str, Any] | None:
        rows = self._query(
            """
            SELECT gateway_id, node_id, sequence_no, uptime_ms,
                   inside_temperature, inside_humidity,
                   outside_temperature, outside_humidity,
                   tilted, status_flags, received_at
            FROM sensor_readings
            WHERE gateway_id = %s AND node_id = %s
            ORDER BY received_at DESC, id DESC
            LIMIT 1
            """,
            (gateway_id, node_id),
        )
        return rows[0] if rows else None

    def latest_minute_aggregate(self, gateway_id: str, node_id: int) -> dict[str, Any] | None:
        rows = self._query(_minute_select_sql("LIMIT 1"), (gateway_id, node_id))
        return rows[0] if rows else None

    def minute_history(self, gateway_id: str, node_id: int, limit: int = 1440) -> list[dict[str, Any]]:
        if not 1 <= limit <= 10_080:
            raise ValueError("limit must be between 1 and 10080")
        return self._query(_minute_select_sql("LIMIT %s"), (gateway_id, node_id, limit))

    def minute_export_rows(
        self,
        gateway_id: str,
        node_id: int,
        start: Any | None = None,
        end: Any | None = None,
    ) -> list[dict[str, Any]]:
        """Return full minute aggregates (all columns) ordered for CSV export."""
        clauses = ["gateway_id = %s", "node_id = %s"]
        parameters: list[object] = [gateway_id, node_id]
        if start is not None:
            clauses.append("bucket_start >= %s")
            parameters.append(start)
        if end is not None:
            clauses.append("bucket_start <= %s")
            parameters.append(end)
        statement = f"""
            SELECT {', '.join(_MINUTE_AGGREGATE_COLUMNS)}
            FROM sensor_minute_aggregates
            WHERE {' AND '.join(clauses)}
            ORDER BY bucket_start ASC
        """
        return self._query(statement, tuple(parameters))

    def minute_export_count(self, gateway_id: str, node_id: int) -> int:
        """Count exportable minute rows before rendering the CSV payload."""
        rows = self._query(
            """
            SELECT COUNT(*) AS total
            FROM sensor_minute_aggregates
            WHERE gateway_id = %s AND node_id = %s
            """,
            (gateway_id, node_id),
        )
        return int(rows[0]["total"]) if rows else 0

    def minute_quality_summary(
        self,
        gateway_id: str,
        node_id: int,
        start: Any | None = None,
        end: Any | None = None,
    ) -> dict[str, Any]:
        """Summarize minute-aggregate coverage and sensor validity for one node."""
        clauses = ["gateway_id = %s", "node_id = %s"]
        parameters: list[object] = [gateway_id, node_id]
        if start is not None:
            clauses.append("bucket_start >= %s")
            parameters.append(start)
        if end is not None:
            clauses.append("bucket_start <= %s")
            parameters.append(end)
        where_sql = " AND ".join(clauses)
        rows = self._query(
            f"""
            SELECT
                COUNT(*) AS minute_rows,
                MIN(bucket_start) AS first_bucket_start,
                MAX(bucket_start) AS last_bucket_start,
                SUM(sample_count) AS total_samples,
                SUM(inside_temperature_valid_count) AS inside_temperature_valid_samples,
                SUM(inside_humidity_valid_count) AS inside_humidity_valid_samples,
                SUM(outside_temperature_valid_count) AS outside_temperature_valid_samples,
                SUM(outside_humidity_valid_count) AS outside_humidity_valid_samples,
                SUM(tilt_valid_count) AS tilt_valid_samples,
                SUM(tilt_alarm_count) AS tilt_alarm_samples
            FROM sensor_minute_aggregates
            WHERE {where_sql}
            """,
            tuple(parameters),
        )
        summary = dict(rows[0]) if rows else {}
        minute_rows = int(summary.get("minute_rows") or 0)
        summary["minute_rows"] = minute_rows
        if minute_rows > 0:
            summary["expected_minute_rows"] = _expected_minute_rows(
                summary.get("first_bucket_start"), summary.get("last_bucket_start")
            )
            summary["missing_minute_rows"] = max(
                0, int(summary["expected_minute_rows"]) - minute_rows
            )
            summary["missing_minute_ratio"] = round(
                summary["missing_minute_rows"] / int(summary["expected_minute_rows"]), 4
            )
            for field, total_key in (
                ("inside_temperature_valid_ratio", "total_samples"),
                ("inside_humidity_valid_ratio", "total_samples"),
                ("outside_temperature_valid_ratio", "total_samples"),
                ("outside_humidity_valid_ratio", "total_samples"),
            ):
                valid_key = field.replace("_valid_ratio", "_valid_samples")
                summary[field] = _valid_ratio(summary.get(valid_key), summary.get(total_key))
            summary["tilt_alarm_ratio"] = _valid_ratio(
                summary.get("tilt_alarm_samples"), summary.get("tilt_valid_samples")
            )
        for key, value in list(summary.items()):
            if isinstance(value, Decimal):
                summary[key] = float(value)
        return summary

    def history(self, gateway_id: str, node_id: int, limit: int = 100) -> list[dict[str, Any]]:
        if not 1 <= limit <= 500:
            raise ValueError("limit must be between 1 and 500")
        return self._query(
            """
            SELECT gateway_id, node_id, sequence_no, uptime_ms,
                   inside_temperature, inside_humidity,
                   outside_temperature, outside_humidity,
                   tilted, status_flags, received_at
            FROM sensor_readings
            WHERE gateway_id = %s AND node_id = %s
            ORDER BY received_at DESC, id DESC
            LIMIT %s
            """,
            (gateway_id, node_id, limit),
        )

    def _execute_insert(self, statement: str, parameters: tuple[object, ...]) -> bool:
        with self._lock:
            cursor = self._connection.cursor()
            try:
                cursor.execute(statement, parameters)
                self._connection.commit()
                return cursor.rowcount == 1
            except Exception:
                self._connection.rollback()
                raise
            finally:
                cursor.close()

    def _query(self, statement: str, parameters: tuple[object, ...]) -> list[dict[str, Any]]:
        with self._lock:
            try:
                cursor = self._connection.cursor(dictionary=True)
            except TypeError:
                cursor = self._connection.cursor()
            try:
                cursor.execute(statement, parameters)
                rows = cursor.fetchall()
                if rows and not isinstance(rows[0], dict):
                    columns = [column[0] for column in cursor.description]
                    return [dict(zip(columns, row)) for row in rows]
                return list(rows)
            finally:
                cursor.close()

    def close(self) -> None:
        with self._lock:
            self._connection.close()


def _minute_select_sql(limit_clause: str) -> str:
    return f"""
        SELECT gateway_id, node_id, last_sequence_no AS sequence_no, last_uptime_ms AS uptime_ms,
               inside_temperature_avg AS inside_temperature, inside_humidity_avg AS inside_humidity,
               outside_temperature_avg AS outside_temperature, outside_humidity_avg AS outside_humidity,
               CASE WHEN tilt_alarm_count > 0 THEN 1
                    WHEN tilt_valid_count > 0 THEN 0
                    ELSE NULL END AS tilted,
               status_flags_or AS status_flags, last_received_at AS received_at,
               sample_count, tilt_valid_count, tilt_alarm_count
        FROM sensor_minute_aggregates
        WHERE gateway_id = %s AND node_id = %s
        ORDER BY bucket_start DESC
        {limit_clause}
    """


_MINUTE_AGGREGATE_COLUMNS = (
    "gateway_id", "node_id", "bucket_start", "sample_count",
    "inside_temperature_avg", "inside_temperature_min", "inside_temperature_max",
    "inside_temperature_valid_count",
    "inside_humidity_avg", "inside_humidity_min", "inside_humidity_max",
    "inside_humidity_valid_count",
    "outside_temperature_avg", "outside_temperature_min", "outside_temperature_max",
    "outside_temperature_valid_count",
    "outside_humidity_avg", "outside_humidity_min", "outside_humidity_max",
    "outside_humidity_valid_count",
    "tilt_valid_count", "tilt_alarm_count", "status_flags_or",
    "last_sequence_no", "last_uptime_ms", "last_received_at",
)


def _expected_minute_rows(first_bucket_start: Any, last_bucket_start: Any) -> int:
    """Upper-bound row count from first to last bucket, inclusive, at 1-minute cadence."""
    if first_bucket_start is None or last_bucket_start is None:
        return 0
    span_seconds = (last_bucket_start - first_bucket_start).total_seconds()
    return int(span_seconds // 60) + 1


def _valid_ratio(valid_samples: Any, total_samples: Any) -> float | None:
    if total_samples is None or int(total_samples) == 0:
        return None
    return round(float(valid_samples or 0) / float(total_samples), 4)


def open_mysql_storage(settings: Settings, connector: Any | None = None) -> MySqlStorage:
    """Open the configured MySQL connection with transactions enabled."""
    if connector is None:
        import mysql.connector

        connector = mysql.connector

    connection = connector.connect(
        host=settings.database_host,
        port=settings.database_port,
        database=settings.database_name,
        user=settings.database_user,
        password=settings.database_password,
        autocommit=False,
    )
    return MySqlStorage(connection)
