# Beehive Monitoring Web Backend

This service receives validated MQTT telemetry from ESP32 gateways, writes it to MySQL, and exposes query APIs.
The existing `beehive-serial-ingest` command remains available for the earlier USB serial debug workflow. The normal workflow is now MQTT.

## Install

From `F:\0Web`:

```powershell
python -m pip install -e .
```

## Database

For a new database, run `db/bootstrap.sql` as a MySQL administrator.

For an existing serial-only `beehive_gateway` database, run this migration once before starting the MQTT service:

```powershell
Get-Content .\db\migrations\001_add_mqtt_gateway_metadata.sql |
  & 'F:\MYsql\bin\mysql.exe' -u root -p
```

The migration adds the `gateways` table, adds `gateway_id` to `sensor_readings`, and changes deduplication to `(gateway_id, node_id, sequence_no)`.

For the real-time cache and minute aggregation data path, run this additional migration once:

```powershell
Get-Content .\db\migrations\002_add_minute_aggregates.sql |
  & 'F:\MYsql\bin\mysql.exe' -u root -p
```

After migration `002`, new telemetry is held in memory for the real-time page and only one aggregate row per gateway/node/minute is written to `sensor_minute_aggregates`. Existing `sensor_readings` data remains unchanged.

## Configuration

Set runtime secrets only in the PowerShell session that starts the process. Do not commit them.

```powershell
$env:BEEHIVE_DB_HOST = "127.0.0.1"
$env:BEEHIVE_DB_PORT = "3306"
$env:BEEHIVE_DB_NAME = "beehive_gateway"
$env:BEEHIVE_DB_USER = "beehive_app"
$env:BEEHIVE_DB_PASSWORD = "replace-with-your-database-password"

$env:BEEHIVE_MQTT_URI = "mqtt://broker.emqx.io:1883"
$env:BEEHIVE_MQTT_USERNAME = ""
$env:BEEHIVE_MQTT_PASSWORD = ""
$env:BEEHIVE_MQTT_CLIENT_ID = "beehive_web"
$env:BEEHIVE_MQTT_TOPIC_PREFIX = "beehive/v1"
$env:BEEHIVE_MQTT_KEEPALIVE = "30"
$env:BEEHIVE_MQTT_RECONNECT_DELAY = "3"

$env:BEEHIVE_HTTP_HOST = "127.0.0.1"
$env:BEEHIVE_HTTP_PORT = "8000"
```

`broker.emqx.io` is a public test broker. The service treats every MQTT message as untrusted input and rejects invalid topics, malformed JSON, non-finite values, out-of-range measurements, and gateway/node mismatches.

巴法云自定义主题模式：将 `BEEHIVE_MQTT_URI` 设为 `mqtt://mqttv2.bemfa.com:2023`，填写 MQTT 用户名和密码，并设置 `BEEHIVE_MQTT_TOPIC_MODE=bemfa`、`BEEHIVE_MQTT_TELEMETRY_TOPIC=beehivegw001node001telemetry`、`BEEHIVE_MQTT_STATUS_TOPIC=beehivegw001status`、`BEEHIVE_MQTT_GATEWAY_ID=gw-001`、`BEEHIVE_MQTT_FLAT_NODE_ID=1`。密码只放在本机 `.env`，不要提交或粘贴到日志。

## Run

Start the API and MQTT worker together:

```powershell
beehive-web
```

Open `http://127.0.0.1:8000/docs` for the interactive API documentation.

## 一键启动（Windows EXE）

已构建的一键启动程序位于 `dist\\BeehiveMonitor.exe`。它会启动 FastAPI 与 MQTT worker，待服务就绪后自动在默认浏览器打开监控页面。

首次使用前，在 `F:\0Web\\dist` 中执行：

```powershell
Copy-Item .\BeehiveMonitor.env.example .\.env
notepad .\.env
```

只需将 `.env` 中的 `BEEHIVE_DB_PASSWORD=replace-with-your-password` 改为 `beehive_app` 数据库用户的实际密码，其他默认配置保持不变即可。`.env` 必须与 `BeehiveMonitor.exe` 放在同一目录，且不得提交、分享或上传。

启动前先在旧的 `beehive-web` PowerShell 窗口按 `Ctrl+C` 停止服务，避免占用 `127.0.0.1:8000`。之后双击 `BeehiveMonitor.exe`；浏览器自动打开 `http://127.0.0.1:8000/` 即表示启动完成。程序的控制台窗口需保持打开，关闭该窗口会停止 Web 服务与 MQTT 接收。

需要重新打包时，在 `F:\0Web` 中运行：

```powershell
.\scripts\build_launcher.ps1
```

构建脚本会排除 Anaconda 中冲突的 Qt 绑定，并在 PyInstaller 失败时中止，不会输出错误的成功提示。

The worker subscribes with QoS 1 to:

```text
beehive/v1/gateways/+/nodes/+/telemetry
beehive/v1/gateways/+/status
```

MQTT disconnects and invalid messages are logged. They do not stop the HTTP process. MQTT QoS 1 duplicates are deduplicated by the database unique key.

Run only the MQTT worker without HTTP when needed:

```powershell
beehive-mqtt-ingest
```

## API

```text
GET /api/health
GET /api/gateways
GET /api/gateways/{gateway_id}
GET /api/gateways/{gateway_id}/status
GET /api/gateways/{gateway_id}/nodes
GET /api/gateways/{gateway_id}/nodes/{node_id}/latest
GET /api/gateways/{gateway_id}/nodes/{node_id}/history?limit=100
GET /api/gateways/{gateway_id}/nodes/{node_id}/minute-history?limit=1440
GET /api/gateways/{gateway_id}/nodes/{node_id}/minute-history/export?hours=168&start=&end=
GET /api/gateways/{gateway_id}/nodes/{node_id}/quality?hours=24
```

节点列表和最新数据接口还会返回节点接收新鲜度字段：`status`（`online`、`stale`、`offline` 或 `unknown`）、`last_seen_at` 和 `last_seen_age_seconds`。节点没有独立在线状态消息，因此这里表示“网关最近是否收到该节点 telemetry”；超过 10 秒标记为通信不稳定，超过 60 秒标记为离线，页面会保留最后测量值并注明已过期。

`minute-history/export` 返回分钟聚合数据的 CSV 附件（UTF-8 BOM，Excel 可直接打开）。默认 `format=lean`，精简 7 列训练格式：`bucket_start, sample_count, inside_temperature, inside_humidity, outside_temperature, outside_humidity, tilt_alarm`（四路温湿度为该分钟均值，倾倒告警为 0/1 标记，空缺传感器值为空单元格）。`format=full` 输出与 `sensor_minute_aggregates` 一致的 26 列完整格式（含 min/max/有效样本数等质量字段）。`hours`（默认 168）、`start`、`end` 三者可组合，`start`/`end` 为 ISO 时间。精简格式可直接作为数据分析或温湿度预测模型的训练数据。

`quality` 返回指定窗口（默认 24 小时，上限 720 小时）内的分钟覆盖率统计：`minute_rows`、`expected_minute_rows`（按首末桶时间推算）、`missing_minute_rows`、`missing_minute_ratio`，各路温湿度的有效率（`*_valid_ratio`，基于有效样本计数），以及倾倒告警次数与占比。窗口内无数据时只返回 `minute_rows: 0`。

## Test

```powershell
python -m pytest
```
