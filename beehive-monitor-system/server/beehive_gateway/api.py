"""HTTP API for gateway status and sensor telemetry."""

from __future__ import annotations

import logging
from contextlib import asynccontextmanager
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import FileResponse, Response
from fastapi.staticfiles import StaticFiles

from beehive_gateway.config import load_web_settings
from beehive_gateway.csv_export import build_csv_filename, render_minute_aggregates_csv
from beehive_gateway.mqtt_ingest import MqttReceiver
from beehive_gateway.node_presence import get_node_presence
from beehive_gateway.storage import MySqlStorage, open_mysql_storage
from beehive_gateway.telemetry_state import TelemetryState


LOGGER = logging.getLogger(__name__)
WEB_DIR = Path(__file__).resolve().parent / "web"


def create_app(
    storage: Any | None = None,
    receiver: MqttReceiver | None = None,
    telemetry_state: TelemetryState | None = None,
) -> FastAPI:
    """Create the API; dependency injection keeps tests independent of MySQL/MQTT."""
    @asynccontextmanager
    async def lifespan(app: FastAPI):
        if app.state.storage is None:
            settings = load_web_settings()
            app.state.storage = open_mysql_storage(settings)
            app.state.owns_storage = True
            app.state.receiver = MqttReceiver(settings, app.state.storage)
            app.state.telemetry_state = app.state.receiver.telemetry_state
        if app.state.receiver is not None:
            app.state.receiver.start()
        try:
            yield
        finally:
            if app.state.receiver is not None:
                app.state.receiver.stop()
            if app.state.owns_storage and app.state.storage is not None:
                app.state.storage.close()

    app = FastAPI(title="Beehive Gateway API", version="0.1.0", lifespan=lifespan)
    app.state.storage = storage
    app.state.receiver = receiver
    app.state.telemetry_state = telemetry_state or (receiver.telemetry_state if receiver is not None else TelemetryState())
    app.state.owns_storage = False

    if WEB_DIR.is_dir():
        app.mount("/static", StaticFiles(directory=str(WEB_DIR)), name="static")

        @app.get("/", include_in_schema=False)
        def index() -> FileResponse:
            index_path = WEB_DIR / "index.html"
            if not index_path.is_file():
                raise HTTPException(status_code=404, detail="index.html not found")
            return FileResponse(str(index_path))

    @app.get("/api/health")
    def health() -> dict[str, str]:
        return {"status": "ok"}

    @app.get("/api/gateways")
    def gateways() -> list[dict[str, Any]]:
        return app.state.storage.list_gateways()

    @app.get("/api/gateways/{gateway_id}")
    def gateway(gateway_id: str) -> dict[str, Any]:
        result = app.state.storage.get_gateway(gateway_id)
        if result is None:
            raise HTTPException(status_code=404, detail="gateway not found")
        return result

    @app.get("/api/gateways/{gateway_id}/status")
    def gateway_status(gateway_id: str) -> dict[str, Any]:
        result = app.state.storage.get_gateway(gateway_id)
        if result is None:
            raise HTTPException(status_code=404, detail="gateway not found")
        return result

    @app.get("/api/gateways/{gateway_id}/nodes")
    def nodes(gateway_id: str) -> list[dict[str, Any]]:
        node_ids: set[int] = set()
        node_last_seen: dict[int, Any] = {}
        for row in app.state.storage.list_nodes(gateway_id):
            node_id = int(row["node_id"])
            node_ids.add(node_id)
            last_seen_at = row.get("last_seen_at")
            if last_seen_at is not None:
                node_last_seen[node_id] = last_seen_at
        list_minute_nodes = getattr(app.state.storage, "list_minute_nodes", None)
        if list_minute_nodes is not None:
            for row in list_minute_nodes(gateway_id):
                node_id = int(row["node_id"])
                node_ids.add(node_id)
                last_seen_at = row.get("last_seen_at")
                current_last_seen = node_last_seen.get(node_id)
                if last_seen_at is not None and (
                    current_last_seen is None or _timestamp_is_newer(last_seen_at, current_last_seen)
                ):
                    node_last_seen[node_id] = last_seen_at
        node_ids.update(app.state.telemetry_state.node_ids(gateway_id))
        result = []
        for node_id in sorted(node_ids):
            snapshot = app.state.telemetry_state.latest(gateway_id, node_id)
            last_seen_at = snapshot.received_at if snapshot is not None else node_last_seen.get(node_id)
            result.append({"gateway_id": gateway_id, "node_id": node_id, **get_node_presence(last_seen_at)})
        return result

    @app.get("/api/gateways/{gateway_id}/nodes/{node_id}/latest")
    def latest(gateway_id: str, node_id: int) -> dict[str, Any]:
        snapshot = app.state.telemetry_state.latest(gateway_id, node_id)
        if snapshot is not None:
            reading = snapshot.reading
            return {
                "gateway_id": gateway_id, "node_id": node_id, "sequence_no": reading.sequence_no,
                "uptime_ms": reading.uptime_ms, "inside_temperature": reading.inside_temperature,
                "inside_humidity": reading.inside_humidity, "outside_temperature": reading.outside_temperature,
                "outside_humidity": reading.outside_humidity, "tilted": reading.tilted,
                "status_flags": reading.status_flags, "received_at": snapshot.received_at,
                **get_node_presence(snapshot.received_at),
            }
        lookup = getattr(app.state.storage, "latest_minute_aggregate", app.state.storage.latest_reading)
        result = lookup(gateway_id, node_id)
        if result is None and lookup is not app.state.storage.latest_reading:
            result = app.state.storage.latest_reading(gateway_id, node_id)
        if result is None:
            raise HTTPException(status_code=404, detail="reading not found")
        return {**result, **get_node_presence(result.get("received_at"))}

    @app.get("/api/gateways/{gateway_id}/nodes/{node_id}/minute-history")
    def minute_history(
        gateway_id: str,
        node_id: int,
        limit: int = Query(default=1440, ge=1, le=10_080),
    ) -> list[dict[str, Any]]:
        return app.state.storage.minute_history(gateway_id, node_id, limit)

    @app.get("/api/gateways/{gateway_id}/nodes/{node_id}/history")
    def history(
        gateway_id: str,
        node_id: int,
        limit: int = Query(default=100, ge=1, le=500),
    ) -> list[dict[str, Any]]:
        return app.state.storage.history(gateway_id, node_id, limit)

    @app.get("/api/gateways/{gateway_id}/nodes/{node_id}/minute-history/export")
    def export_minute_history(
        gateway_id: str,
        node_id: int,
        hours: int = Query(default=24 * 7, ge=1, le=24 * 365),
        start: datetime | None = Query(default=None),
        end: datetime | None = Query(default=None),
        format: str = Query(default="lean", pattern="^(lean|full)$"),
    ) -> Response:
        end_time = _local_naive(end) or datetime.now()
        start_time = _local_naive(start) or end_time - timedelta(hours=hours)
        if start is not None and end is not None and start_time >= end_time:
            raise HTTPException(status_code=422, detail="start must be earlier than end")
        rows = app.state.storage.minute_export_rows(
            gateway_id, node_id, start=start_time, end=end_time
        )
        lean = format == "lean"
        payload = render_minute_aggregates_csv(rows, lean=lean).encode("utf-8")
        filename = build_csv_filename(gateway_id, node_id, variant="" if lean else "_full")
        return Response(
            content=payload,
            media_type="text/csv; charset=utf-8",
            headers={"Content-Disposition": f'attachment; filename="{filename}"'},
        )

    @app.get("/api/gateways/{gateway_id}/nodes/{node_id}/quality")
    def node_quality(
        gateway_id: str,
        node_id: int,
        hours: int = Query(default=24, ge=1, le=24 * 30),
    ) -> dict[str, Any]:
        end_time = datetime.now()
        start_time = end_time - timedelta(hours=hours)
        return app.state.storage.minute_quality_summary(
            gateway_id, node_id, start=start_time, end=end_time
        )

    return app


def _timestamp_is_newer(candidate: Any, current: Any) -> bool:
    """Compare database timestamps while tolerating naive/aware datetime mixes."""
    try:
        return candidate > current
    except TypeError:
        candidate_naive = candidate.replace(tzinfo=None)
        current_naive = current.replace(tzinfo=None)
        return candidate_naive > current_naive


def _local_naive(value: datetime | None) -> datetime | None:
    """Align query bounds with bucket_start storage: naive local wall-clock time."""
    if value is None or value.tzinfo is None:
        return value
    return value.astimezone().replace(tzinfo=None)


app = create_app()
