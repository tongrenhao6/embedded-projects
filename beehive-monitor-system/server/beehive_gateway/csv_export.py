"""Minute-aggregate CSV export for training-set preparation."""

from __future__ import annotations

import csv
import io
import re
from datetime import date, datetime
from typing import Any

# 精简（训练）格式：时间 + 样本数 + 四路温湿度均值 + 倾倒告警标记。
LEAN_EXPORT_COLUMNS: tuple[str, ...] = (
    "bucket_start",
    "sample_count",
    "inside_temperature",
    "inside_humidity",
    "outside_temperature",
    "outside_humidity",
    "tilt_alarm",
)

_LEAN_SOURCE_FIELDS = {
    "inside_temperature": "inside_temperature_avg",
    "inside_humidity": "inside_humidity_avg",
    "outside_temperature": "outside_temperature_avg",
    "outside_humidity": "outside_humidity_avg",
}

# 完整格式：与数据库列一致的导出列；空缺字段输出空字符串，便于 pandas 直接读取。
FULL_EXPORT_COLUMNS: tuple[str, ...] = (
    "gateway_id",
    "node_id",
    "bucket_start",
    "sample_count",
    "inside_temperature_avg",
    "inside_temperature_min",
    "inside_temperature_max",
    "inside_temperature_valid_count",
    "inside_humidity_avg",
    "inside_humidity_min",
    "inside_humidity_max",
    "inside_humidity_valid_count",
    "outside_temperature_avg",
    "outside_temperature_min",
    "outside_temperature_max",
    "outside_temperature_valid_count",
    "outside_humidity_avg",
    "outside_humidity_min",
    "outside_humidity_max",
    "outside_humidity_valid_count",
    "tilt_valid_count",
    "tilt_alarm_count",
    "status_flags_or",
    "last_sequence_no",
    "last_uptime_ms",
    "last_received_at",
)

# 兼容旧名
EXPORT_COLUMNS = FULL_EXPORT_COLUMNS


def render_minute_aggregates_csv(rows: list[dict[str, Any]], lean: bool = True) -> str:
    """Render minute-aggregate rows as RFC 4180 CSV text with a UTF-8 BOM.

    lean=True 输出训练用精简列；lean=False 输出与数据库一致的完整列。
    """
    buffer = io.StringIO()
    writer = csv.writer(buffer, lineterminator="\r\n")
    if lean:
        writer.writerow(LEAN_EXPORT_COLUMNS)
        for row in rows:
            writer.writerow([_lean_cell(column, row) for column in LEAN_EXPORT_COLUMNS])
    else:
        writer.writerow(FULL_EXPORT_COLUMNS)
        for row in rows:
            writer.writerow([_format_cell(row.get(column)) for column in FULL_EXPORT_COLUMNS])
    # Excel 双击打开时依赖 BOM 识别 UTF-8；普通解析器会跳过它。
    return "\ufeff" + buffer.getvalue()


def build_csv_filename(
    gateway_id: str,
    node_id: int,
    generated_at: datetime | None = None,
    variant: str = "",
) -> str:
    """Build a download filename safe for common operating systems."""
    safe_gateway = re.sub(r"[^A-Za-z0-9_-]", "_", gateway_id) or "gateway"
    stamp = (generated_at or datetime.now()).strftime("%Y%m%d_%H%M%S")
    return f"minute_aggregates{variant}_{safe_gateway}_node{node_id}_{stamp}.csv"


def _lean_cell(column: str, row: dict[str, Any]) -> str:
    if column == "tilt_alarm":
        return "1" if int(row.get("tilt_alarm_count") or 0) > 0 else "0"
    source = "bucket_start" if column == "bucket_start" else (
        "sample_count" if column == "sample_count" else _LEAN_SOURCE_FIELDS[column]
    )
    return _format_cell(row.get(source))


def _format_cell(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, datetime):
        text = value.strftime("%Y-%m-%d %H:%M:%S")
        if value.microsecond:
            text += f".{value.microsecond // 1000:03d}"
        return text
    if isinstance(value, date):
        return value.isoformat()
    return str(value)
