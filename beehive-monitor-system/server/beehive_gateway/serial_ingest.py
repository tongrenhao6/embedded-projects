"""Serial line ingestion for ESP32 gateway GWJSON output."""

from __future__ import annotations

import logging
import time
from dataclasses import dataclass
from typing import Iterable, Protocol

from beehive_gateway.config import ConfigurationError, load_settings
from beehive_gateway.protocol import GatewayFrameError, SensorReading, parse_gateway_line
from beehive_gateway.storage import open_mysql_storage


LOGGER = logging.getLogger(__name__)


class ReadingStorage(Protocol):
    def store(self, reading: SensorReading) -> bool: ...


@dataclass
class IngestStats:
    ignored_lines: int = 0
    invalid_frames: int = 0
    inserted: int = 0
    duplicates: int = 0
    storage_errors: int = 0


def ingest_lines(lines: Iterable[str], storage: ReadingStorage) -> IngestStats:
    """Process lines without allowing one malformed input to stop the batch."""
    stats = IngestStats()
    for line in lines:
        try:
            reading = parse_gateway_line(line.rstrip("\r\n"))
        except GatewayFrameError as error:
            stats.invalid_frames += 1
            LOGGER.warning("Rejected GWJSON frame: %s", error)
            continue

        if reading is None:
            stats.ignored_lines += 1
            continue

        try:
            if storage.store(reading):
                stats.inserted += 1
                LOGGER.info(
                    "Stored sensor reading node_id=%d sequence=%d",
                    reading.node_id,
                    reading.sequence_no,
                )
            else:
                stats.duplicates += 1
                LOGGER.info(
                    "Ignored duplicate sensor reading node_id=%d sequence=%d",
                    reading.node_id,
                    reading.sequence_no,
                )
        except Exception:
            stats.storage_errors += 1
            LOGGER.exception(
                "Could not store GWJSON frame node_id=%d sequence=%d",
                reading.node_id,
                reading.sequence_no,
            )
    return stats


def read_serial_forever(
    port: str,
    baudrate: int,
    storage: ReadingStorage,
    reconnect_delay_seconds: float = 2.0,
) -> None:
    """Read one UTF-8 serial line at a time and reconnect after I/O errors."""
    import serial

    while True:
        try:
            with serial.Serial(port, baudrate=baudrate, timeout=1) as connection:
                LOGGER.info("Reading gateway serial port %s at %d bps", port, baudrate)
                while True:
                    raw_line = connection.readline()
                    if raw_line:
                        ingest_lines([raw_line.decode("utf-8", errors="replace")], storage)
        except serial.SerialException:
            LOGGER.exception("Serial connection failed; retrying in %.1f seconds", reconnect_delay_seconds)
            time.sleep(reconnect_delay_seconds)


def main() -> int:
    """Run the configured foreground serial ingestion process."""
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s %(levelname)s %(name)s: %(message)s",
    )
    try:
        settings = load_settings()
    except ConfigurationError as error:
        LOGGER.error("Invalid configuration: %s", error)
        return 2

    try:
        storage = open_mysql_storage(settings)
    except Exception:
        LOGGER.exception("Could not open MySQL database connection")
        return 1

    try:
        read_serial_forever(settings.serial_port, settings.serial_baud, storage)
    except KeyboardInterrupt:
        LOGGER.info("Serial ingestion stopped by user")
    finally:
        storage.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
