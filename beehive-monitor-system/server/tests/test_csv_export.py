from __future__ import annotations

import csv
import io
from datetime import datetime

from beehive_gateway.csv_export import (
    FULL_EXPORT_COLUMNS,
    LEAN_EXPORT_COLUMNS,
    build_csv_filename,
    render_minute_aggregates_csv,
)


def _sample_row() -> dict[str, object]:
    return {
        "gateway_id": "gw-001",
        "node_id": 1,
        "bucket_start": datetime(2026, 9, 8, 12, 0),
        "sample_count": 30,
        "inside_temperature_avg": 25.5,
        "inside_temperature_min": 25.0,
        "inside_temperature_max": 26.0,
        "inside_temperature_valid_count": 30,
        "inside_humidity_avg": None,
        "inside_humidity_min": None,
        "inside_humidity_max": None,
        "inside_humidity_valid_count": 0,
        "outside_temperature_avg": 24.75,
        "outside_temperature_min": 24.0,
        "outside_temperature_max": 25.5,
        "outside_temperature_valid_count": 28,
        "outside_humidity_avg": None,
        "outside_humidity_min": None,
        "outside_humidity_max": None,
        "outside_humidity_valid_count": 0,
        "tilt_valid_count": 30,
        "tilt_alarm_count": 1,
        "status_flags_or": 15,
        "last_sequence_no": 30,
        "last_uptime_ms": 60000,
        "last_received_at": datetime(2026, 9, 8, 12, 0, 58, 123000),
    }


def _rows_of(text: str) -> tuple[list[str], list[list[str]]]:
    reader = csv.reader(io.StringIO(text.lstrip("\ufeff")))
    header = next(reader)
    return header, [row for row in reader]


def test_lean_csv_has_training_columns_and_tilt_flag() -> None:
    header, rows = _rows_of(render_minute_aggregates_csv([_sample_row()], lean=True))

    assert header == list(LEAN_EXPORT_COLUMNS)
    assert rows == [[
        "2026-09-08 12:00:00", "30", "25.5", "", "24.75", "", "1"
    ]]


def test_lean_csv_tilt_alarm_zero_when_no_alarm_samples() -> None:
    row = _sample_row()
    row["tilt_alarm_count"] = 0

    _, rows = _rows_of(render_minute_aggregates_csv([row], lean=True))

    assert rows[0][LEAN_EXPORT_COLUMNS.index("tilt_alarm")] == "0"


def test_full_csv_keeps_database_columns_and_null_blanks() -> None:
    header, rows = _rows_of(render_minute_aggregates_csv([_sample_row()], lean=False))

    assert header == list(FULL_EXPORT_COLUMNS)
    values = dict(zip(header, rows[0]))
    assert values["inside_temperature_avg"] == "25.5"
    assert values["inside_humidity_avg"] == ""
    assert values["bucket_start"] == "2026-09-08 12:00:00"
    assert values["last_received_at"] == "2026-09-08 12:00:58.123"


def test_csv_quotes_values_containing_commas() -> None:
    row = _sample_row()
    row["gateway_id"] = "gw,1"

    text = render_minute_aggregates_csv([row], lean=False)

    assert '"gw,1"' in text


def test_build_csv_filename_sanitizes_gateway_id_and_variant() -> None:
    name = build_csv_filename(
        "gw/001:x", 2, generated_at=datetime(2026, 9, 8, 12, 0, 30), variant="_full"
    )

    assert name == "minute_aggregates_full_gw_001_x_node2_20260908_120030.csv"
