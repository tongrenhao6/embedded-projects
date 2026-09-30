from __future__ import annotations

from datetime import datetime, timedelta
from pathlib import Path

from fastapi.testclient import TestClient

from beehive_gateway.api import create_app
from beehive_gateway.config import Settings
from beehive_gateway.mqtt_ingest import MqttReceiver
from beehive_gateway.protocol import SensorReading
from beehive_gateway.telemetry_state import TelemetryState


class FakeStorage:
    def list_gateways(self):
        return [{"gateway_id": "gw-001", "status": "online", "last_seen_at": None}]

    def get_gateway(self, gateway_id: str):
        return {"gateway_id": gateway_id, "status": "online", "last_seen_at": None}

    def list_nodes(self, gateway_id: str):
        return [{"gateway_id": gateway_id, "node_id": 1, "last_seen_at": None}]

    def latest_reading(self, gateway_id: str, node_id: int):
        return {"gateway_id": gateway_id, "node_id": node_id, "sequence_no": 1}

    def history(self, gateway_id: str, node_id: int, limit: int):
        return [{"gateway_id": gateway_id, "node_id": node_id, "sequence_no": 1}]

    def minute_history(self, gateway_id: str, node_id: int, limit: int):
        return [{"gateway_id": gateway_id, "node_id": node_id, "sequence_no": 1, "sample_count": 30}]

    def latest_minute_aggregate(self, gateway_id: str, node_id: int):
        return None

    def minute_export_rows(self, gateway_id: str, node_id: int, start=None, end=None):
        return [{
            "gateway_id": gateway_id,
            "node_id": node_id,
            "bucket_start": datetime(2026, 9, 8, 12, 0),
            "sample_count": 30,
            "inside_temperature_avg": 25.5,
            "inside_humidity_avg": None,
            "last_received_at": datetime(2026, 9, 8, 12, 0, 58),
        }]

    def minute_quality_summary(self, gateway_id: str, node_id: int, start=None, end=None):
        return {
            "gateway_id": gateway_id,
            "node_id": node_id,
            "minute_rows": 60,
            "missing_minute_rows": 1,
            "inside_temperature_valid_ratio": 1.0,
        }


def test_health_and_gateway_queries() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        assert client.get("/api/health").json() == {"status": "ok"}
        assert client.get("/api/gateways").status_code == 200
        node = client.get("/api/gateways/gw-001/nodes").json()[0]
        assert node["node_id"] == 1
        assert node["status"] == "unknown"
        assert client.get("/api/gateways/gw-001/nodes/1/latest").json()["sequence_no"] == 1
        assert client.get("/api/gateways/gw-001/nodes/1/history").status_code == 200


def test_frontend_page_and_static_assets() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        response = client.get("/")
        assert response.status_code == 200
        assert "蜂箱环境监测系统" in response.text
        assert client.get("/static/styles.css").status_code == 200
        assert client.get("/static/app.js").status_code == 200


def test_latest_prefers_live_telemetry_cache_and_minute_history_is_available() -> None:
    state = TelemetryState()
    state.ingest(
        "gw-001",
        SensorReading(1, 99, 198000, 26.0, 51.0, 25.0, 52.0, False, 7),
        datetime.now(),
    )
    with TestClient(create_app(storage=FakeStorage(), telemetry_state=state)) as client:
        latest = client.get("/api/gateways/gw-001/nodes/1/latest").json()
        history = client.get("/api/gateways/gw-001/nodes/1/minute-history?limit=20").json()
        nodes = client.get("/api/gateways/gw-001/nodes").json()

    assert latest["sequence_no"] == 99
    assert latest["inside_temperature"] == 26.0
    assert history[0]["sample_count"] == 30
    assert nodes[0]["gateway_id"] == "gw-001"
    assert nodes[0]["node_id"] == 1
    assert nodes[0]["status"] == "online"
    assert nodes[0]["last_seen_age_seconds"] == 0.0


def test_nodes_use_database_last_seen_when_memory_cache_is_empty() -> None:
    class HistoricalStorage(FakeStorage):
        def list_nodes(self, gateway_id: str):
            return [{
                "gateway_id": gateway_id,
                "node_id": 1,
                "last_seen_at": datetime.now() - timedelta(seconds=61),
            }]

    with TestClient(create_app(storage=HistoricalStorage())) as client:
        node = client.get("/api/gateways/gw-001/nodes").json()[0]

    assert node["status"] == "offline"
    assert node["last_seen_age_seconds"] >= 61.0


def test_frontend_preserves_telemetry_load_errors() -> None:
    script = (Path(__file__).resolve().parents[1] / "beehive_gateway" / "web" / "app.js").read_text(
        encoding="utf-8"
    )
    assert "await loadNodes(preserveSelection);\n      clearError();" not in script


def test_frontend_recognizes_mysql_boolean_tilt_alarm() -> None:
    script = (Path(__file__).resolve().parents[1] / "beehive_gateway" / "web" / "app.js").read_text(
        encoding="utf-8"
    )
    assert "data.tilted === true || data.tilted === 1" in script


def test_frontend_chart_hover_reads_current_geometry() -> None:
    script = (Path(__file__).resolve().parents[1] / "beehive_gateway" / "web" / "app.js").read_text(
        encoding="utf-8"
    )
    # 悬停处理器必须从 canvas._chartState 读取当次绘制的几何与数据；
    # 旧实现闭包捕获首次绘制的 data 长度，切换历史条数后 tooltip 向左偏移。
    assert "canvas._chartState = { data, width, padLeft: pad.left, chartW, onHover };" in script
    assert "const chart = canvas._chartState;" in script


def test_mqtt_start_failure_does_not_stop_http_service() -> None:
    settings = Settings(
        serial_port="",
        serial_baud=115200,
        database_host="127.0.0.1",
        database_port=3306,
        database_name="beehive_gateway",
        database_user="beehive_app",
        database_password="not-a-real-password",
    )

    def broken_client_factory():
        raise RuntimeError("broker unavailable")

    receiver = MqttReceiver(settings, FakeStorage(), client_factory=broken_client_factory)
    with TestClient(create_app(storage=FakeStorage(), receiver=receiver)) as client:
        assert client.get("/api/health").json() == {"status": "ok"}


def test_minute_history_export_returns_csv_attachment() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        response = client.get("/api/gateways/gw-001/nodes/1/minute-history/export")

    assert response.status_code == 200
    assert response.headers["content-type"].startswith("text/csv")
    assert "attachment" in response.headers["content-disposition"]
    assert response.headers["content-disposition"].endswith('.csv"')
    assert response.content.startswith(b"\xef\xbb\xbf")
    # 默认精简格式：bucket_start, sample_count, inside_temperature, ...
    assert b"bucket_start,sample_count,inside_temperature,inside_humidity" in response.content
    assert b"2026-09-08 12:00:00,30,25.5" in response.content


def test_minute_history_export_full_format_keeps_database_columns() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        response = client.get(
            "/api/gateways/gw-001/nodes/1/minute-history/export?format=full"
        )

    assert response.status_code == 200
    assert b"gateway_id,node_id,bucket_start,sample_count" in response.content
    assert b"inside_temperature_avg" in response.content
    assert "_full" in response.headers["content-disposition"]


def test_minute_history_export_rejects_unknown_format() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        response = client.get("/api/gateways/gw-001/nodes/1/minute-history/export?format=xml")

    assert response.status_code == 422


def test_minute_history_export_rejects_inverted_range() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        response = client.get(
            "/api/gateways/gw-001/nodes/1/minute-history/export"
            "?start=2026-09-08T12:00:00Z&end=2026-09-08T11:00:00Z"
        )

    assert response.status_code == 422


def test_node_quality_endpoint_returns_summary() -> None:
    with TestClient(create_app(storage=FakeStorage())) as client:
        payload = client.get("/api/gateways/gw-001/nodes/1/quality").json()

    assert payload["minute_rows"] == 60
    assert payload["missing_minute_rows"] == 1
    assert payload["inside_temperature_valid_ratio"] == 1.0
