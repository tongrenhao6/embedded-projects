"""Clear test telemetry data from the beehive_gateway database.

Before truncating, every table is backed up to db/backups/<table>_<stamp>.csv.
Credentials come from the environment or dist/.env (never printed).

Usage: python scripts/clear_test_data.py
"""

from __future__ import annotations

import csv
import os
from datetime import datetime
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
ENV_FILE = PROJECT_ROOT / "dist" / ".env"
BACKUP_DIR = PROJECT_ROOT / "db" / "backups"

TABLES = ("sensor_minute_aggregates", "sensor_readings", "gateways")


def load_env_file() -> None:
    if not ENV_FILE.is_file():
        return
    for line in ENV_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line and not line.startswith("#") and "=" in line:
            key, value = line.split("=", 1)
            os.environ.setdefault(key.strip(), value.strip())


def backup_table(cursor, table: str, stamp: str) -> Path:
    cursor.execute(f"SELECT * FROM {table}")
    columns = [column[0] for column in cursor.description]
    rows = cursor.fetchall()
    BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    backup_path = BACKUP_DIR / f"{table}_{stamp}.csv"
    with backup_path.open("w", newline="", encoding="utf-8-sig") as handle:
        writer = csv.writer(handle)
        writer.writerow(columns)
        writer.writerows(rows)
    return backup_path, len(rows)


def main() -> None:
    load_env_file()
    import mysql.connector

    from beehive_gateway.config import load_web_settings

    settings = load_web_settings()
    connection = mysql.connector.connect(
        host=settings.database_host,
        port=settings.database_port,
        database=settings.database_name,
        user=settings.database_user,
        password=settings.database_password,
    )
    cursor = connection.cursor()
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    print("== 清空前备份 ==")
    counts: dict[str, int] = {}
    for table in TABLES:
        backup_path, row_count = backup_table(cursor, table, stamp)
        counts[table] = row_count
        print(f"  {table}: {row_count} 行 -> {backup_path.name}")

    print("== 清空 ==")
    for table in TABLES:
        try:
            cursor.execute(f"TRUNCATE TABLE {table}")
        except mysql.connector.Error as error:
            print(f"  TRUNCATE {table} 失败（{error}），改用 DELETE")
            cursor.execute(f"DELETE FROM {table}")
    connection.commit()

    print("== 清空后核对 ==")
    for table in TABLES:
        cursor.execute(f"SELECT COUNT(*) FROM {table}")
        remaining = cursor.fetchone()[0]
        status = "OK" if remaining == 0 else "未清空！"
        print(f"  {table}: 剩余 {remaining} 行 {status}")

    cursor.close()
    connection.close()


if __name__ == "__main__":
    main()
