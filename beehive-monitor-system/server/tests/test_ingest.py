from __future__ import annotations

import logging

from beehive_gateway.config import Settings
from beehive_gateway.serial_ingest import ingest_lines, main


class RecordingStorage:
    def __init__(self, outcomes: list[bool | Exception]) -> None:
        self.outcomes = outcomes
        self.readings = []

    def store(self, reading: object) -> bool:
        self.readings.append(reading)
        outcome = self.outcomes.pop(0)
        if isinstance(outcome, Exception):
            raise outcome
        return outcome


VALID_LINE = (
    'GWJSON {"node_id":1,"sequence":11,"uptime_ms":23906,'
    '"inside_temperature":27.35,"inside_humidity":47.82,'
    '"outside_temperature":26.64,"outside_humidity":52.00,'
    '"tilted":true,"status_flags":15}'
)


def test_ignores_normal_log_and_writes_valid_gateway_line() -> None:
    storage = RecordingStorage([True])

    stats = ingest_lines(["I (1) app: heartbeat", VALID_LINE], storage)

    assert stats.ignored_lines == 1
    assert stats.inserted == 1
    assert stats.duplicates == 0
    assert stats.invalid_frames == 0
    assert len(storage.readings) == 1


def test_invalid_json_is_logged_and_next_valid_line_is_stored(caplog) -> None:
    storage = RecordingStorage([True])

    with caplog.at_level(logging.WARNING):
        stats = ingest_lines(["GWJSON {bad json}", VALID_LINE], storage)

    assert stats.invalid_frames == 1
    assert stats.inserted == 1
    assert "Rejected GWJSON frame" in caplog.text


def test_duplicate_sequence_is_counted_without_failure() -> None:
    storage = RecordingStorage([False])

    stats = ingest_lines([VALID_LINE], storage)

    assert stats.inserted == 0
    assert stats.duplicates == 1


def test_storage_error_is_logged_and_later_line_is_stored(caplog) -> None:
    storage = RecordingStorage([RuntimeError("MySQL unavailable"), True])

    with caplog.at_level(logging.ERROR):
        stats = ingest_lines([VALID_LINE, VALID_LINE.replace('"sequence":11', '"sequence":12')], storage)

    assert stats.storage_errors == 1
    assert stats.inserted == 1
    assert "Could not store GWJSON frame" in caplog.text


def test_main_opens_storage_runs_serial_loop_and_closes_connection(monkeypatch) -> None:
    settings = Settings(
        serial_port="COM7",
        serial_baud=115200,
        database_host="127.0.0.1",
        database_port=3306,
        database_name="beehive_gateway",
        database_user="beehive_app",
        database_password="not-a-real-password",
    )
    calls: list[object] = []

    class ClosableStorage:
        def close(self) -> None:
            calls.append("closed")

    storage = ClosableStorage()
    monkeypatch.setattr("beehive_gateway.serial_ingest.load_settings", lambda: settings)
    monkeypatch.setattr("beehive_gateway.serial_ingest.open_mysql_storage", lambda value: storage)
    monkeypatch.setattr(
        "beehive_gateway.serial_ingest.read_serial_forever",
        lambda port, baudrate, value: calls.append((port, baudrate, value)),
    )

    assert main() == 0
    assert calls == [("COM7", 115200, storage), "closed"]
