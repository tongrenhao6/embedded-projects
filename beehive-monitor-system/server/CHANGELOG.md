# 更新日志（CHANGELOG）

> 本项目**未使用 Git**，本文件是网页/后端服务逐次改动的流水记录，与 `docs/superpowers/plans|specs/`（历史计划与设计）及 `docs/PROJECT_RECORD.md`（全系统事实档案，**验收结论以它为准**）配合使用。
>
> **维护规则**（含 AI 助手/codex）：每完成一项影响数据协议、MQTT、数据库表、API、页面或部署方式的改动，**当天**在顶部按日期追加小节；只写已完成并验证的事实，计划中的事项不得写成完成。文件时间戳可作为佐证，但推断性结论需标注“（推断）”。
>
> 系统全貌：节点固件 `F:\0Node\Nodeproject` →（LoRa 26 字节帧）→ 网关固件 `F:\0GateWay\myproject` →（MQTT）→ 网页服务（本目录）。

## 2026-09-10

- **修复（图表悬浮偏移）**：折线图 hover 处理器原以 `canvas._hasChartEvents` 只绑定一次，闭包捕获**首次绘制**的 `data`/`pad`/`chartW`；切换历史条数（20→50→100）或更换节点后数据长度变化，索引按旧长度换算导致 tooltip 与垂线向左偏移。改为每次绘制将当前几何与数据写入 `canvas._chartState`，事件处理器动态读取，并按当前显示宽度归一化（兼容两次绘制之间容器尺寸变化）。新增前端回归测试 1 项（合计 75 项通过）。
- **CSV 导出格式精简**：`minute-history/export` 新增 `format` 参数。默认 `lean` 只输出 7 列训练格式（`bucket_start, sample_count, inside_temperature, inside_humidity, outside_temperature, outside_humidity, tilt_alarm`，温湿度为分钟均值、倾倒告警为 0/1）；`format=full` 保留原 26 列数据库镜像（文件名带 `_full` 后缀）。此前每行 26 列的完整镜像对训练场景冗余过大。测试扩展至 78 项。
- **清空测试数据**：三张表（`sensor_minute_aggregates` 228 行、`sensor_readings` 244 行、`gateways` 1 行）确认为测试数据后全部清空，清空前已备份至 `db\backups\*_20260910_095210.csv`。新增可复用脚本 `scripts/clear_test_data.py`（自动备份→清空→核对；`beehive_app` 无 DROP 权限，脚本自动以 DELETE 回退）。自即日起分钟表从零开始积累正式数据。
- `dist\BeehiveMonitor.exe` 已重建（09:54，含精简导出与悬浮修复）并重启，`.env` 沿用；实测空库下 quality 返回 0 行、默认导出为 7 列精简表头。

## 2026-09-08

- **分钟聚合 CSV 导出**：新增 `beehive_gateway/csv_export.py` 与存储层 `minute_export_rows()` / `minute_export_count()`；新增 API `GET /api/gateways/{gateway_id}/nodes/{node_id}/minute-history/export`（默认近 168 小时，支持 `hours` / `start` / `end` 组合，倒序时间范围返回 422）。输出 UTF-8 BOM CSV（RFC 4180、CRLF），列与 `sensor_minute_aggregates` 一致，空缺值输出空单元格，可直接用于 pandas / Excel / 后续模型训练。
- **数据质量统计**：存储层新增 `minute_quality_summary()`；新增 API `GET /api/gateways/{gateway_id}/nodes/{node_id}/quality?hours=24`（1–720 小时）。返回窗口内 `minute_rows`、按首末桶推算的 `expected_minute_rows`、`missing_minute_rows` 与 `missing_minute_ratio`，四路温湿度有效率（基于有效样本计数 / 总样本数），以及倾倒告警次数与占比；窗口无数据时仅返回 `minute_rows: 0`。
- **页面**：明细区新增“导出 CSV”“质量统计”入口，链接跟随当前网关/节点选择，未选中节点时置灰；补齐 `.table-actions` 排版样式。
- **修复**：头部自动刷新文案 5s → 2s（实际轮询 2 秒的遗留不一致）。
- 测试：新增 `tests/test_csv_export.py`（4 项），扩展 `test_api.py`（+3）、`test_storage.py`（+4），合计 74 项全部通过。
- **修复（实机验证发现）**：export/quality 接口原用 UTC 构造时间窗口，而 `bucket_start` 按本地时间存储，导致 24 小时窗口查不到当天数据（差 8 小时）。已改为本地时间，显式传入的 `start`/`end` 带时区时自动转为本地无时区时间。
- `dist\BeehiveMonitor.exe` 已用新代码重建（2026-09-08 20:24，约 216 MB），`.env` 配置沿用，服务已重启并实测：health、导出 CSV、quality 接口均正常。

## 2026-09-02

- **MQTT 从公共 broker（`broker.emqx.io`）切换到巴法云**：
  - `config.py` 新增 `BEEHIVE_MQTT_TOPIC_MODE`（`hierarchical` / `bemfa`）、巴法云平面主题（`BEEHIVE_MQTT_TELEMETRY_TOPIC` / `BEEHIVE_MQTT_STATUS_TOPIC`）、`BEEHIVE_MQTT_GATEWAY_ID` / `BEEHIVE_MQTT_FLAT_NODE_ID` 配置。
  - `mqtt_protocol.py` 增加平面主题解析校验与网关/节点身份映射；`mqtt_ingest.py` 按模式订阅。
  - 补充 `test_mqtt_protocol.py`、`test_mqtt_ingest.py` 用例。
  - 当前平面主题：`beehivegw001node001telemetry`、`beehivegw001status`（网关 `gw-001`、节点 `1`）；新增节点需在巴法云另建主题并扩展接收配置。
- 更新 `README.md`（巴法云配置示例）、`.env.example`、`docs/PROJECT_RECORD.md`（新增 §13）。
- `BeehiveMonitor.spec` 更新，20:27 重建 `dist/BeehiveMonitor.exe`（约 216 MB）；`dist/.env` 同日配置（含数据库密码与巴法云凭据，**机密，不得提交或外传**）。
- 网关侧同步改动见 `F:\0GateWay\myproject\CHANGELOG.md` 同日条目。

## 2026-08-24

- 新增节点在线状态推断 `node_presence.py`：按最后收到 telemetry 的新鲜度判定 `online`（≤10 s）/ `stale`（≤60 s）/ `offline`（>60 s）/ `unknown`（无记录）。
- `storage.py`（回退链：内存快照 → 分钟聚合 → 原始表）、`api.py`（nodes/latest 返回 `status`、`last_seen_at`、`last_seen_age_seconds`）、页面在线徽标同步更新；新增 `test_node_presence.py`，更新 `test_api.py`。
- 局限：只能证明“网关最近收到过该节点数据”，无法区分节点断电与 LoRa 链路中断。

## 2026-08-16

- **实时缓存 + 分钟聚合数据路径**：
  - 新增 `telemetry_state.py`：每个网关/节点仅保留最新 2 s 样本供实时页与倾倒告警；当前分钟内累计温湿度均值/最值/有效样本数，整分钟 UPSERT `sensor_minute_aggregates`；容量上限 256 个网关/节点组合，待写队列为有界重试。
  - MQTT 实时路径不再逐条写 `sensor_readings`（历史数据保留不动作训练集）。
  - 新增迁移 `db/migrations/002_add_minute_aggregates.sql`（**新库需 bootstrap.sql + 002 一起执行**）。
  - 新增 API `/minute-history`；页面图表改用分钟聚合数据，轮询周期 2 s。
- **Windows 一键启动**：新增 `launcher.py`（读 EXE 同目录 `.env` → 启动服务 → 自动打开页面）、`scripts/build_launcher.ps1`（排除 PyQt5/PySide6 冲突），首次构建 `dist/BeehiveMonitor.exe`；`pyproject.toml` 登记入口 `beehive-web` / `beehive-mqtt-ingest` / `beehive-serial-ingest`。
- 08-19 实机确认：`.env` 配置后双击 EXE 启动成功（见 PROJECT_RECORD §10）；MQTT→MySQL 端到端当时仍未确认。

## 2026-08-14（推断，据同批 `bootstrap.sql`/迁移 001 时间戳与 PROJECT_RECORD 记载）

- 搭建 MQTT 接入与 Web 服务骨架：
  - `web_server.py`（uvicorn 入口）、`config.py`（环境变量配置）、`api.py`（FastAPI 路由 + 静态页挂载）、`storage.py`（MySQL 持久化，唯一键幂等写入）、`mqtt_ingest.py`（paho 订阅 worker）、`mqtt_protocol.py`（主题/JSON/数值范围严格校验，MQTT 输入按不可信数据处理）、`beehive_gateway/web/` 监控页面（温湿度卡片、倾倒告警、Canvas 图表、历史表）。
  - 数据库：`db/bootstrap.sql`（`gateways` + `sensor_readings`）、迁移 `001_add_mqtt_gateway_metadata.sql`（幂等键改为 `(gateway_id, node_id, sequence_no)`）。
- （该批文件后经 08-24、09-02 迭代修改，创建批次以上述同批文件时间与计划文档佐证。）

## 2026-08-10

- 首个 PC 端数据通路：`protocol.py`（`GWJSON ` 前缀串口行解析）+ `serial_ingest.py`（pyserial 接入，原始写 `sensor_readings`），附 `test_protocol.py`、`test_ingest.py`；计划与设计文档见 `docs/superpowers/plans|specs/2026-08-10-pc-data-ingest*.md`。
- 现定位：**调试通路**。正常数据路径为 MQTT；注意 serial 的 INSERT 未带 `gateway_id`（依赖表默认值 `legacy`），勿与 MQTT 数据混看。

## 备注（接班须知）

- `index.html` 内“5 秒刷新”文案与实际 2 秒轮询不一致（遗留文案，未改）。
- `/api/.../history` 仍读 `sensor_readings`，但实时路径已不再写入该表；图表用的是 `/minute-history`，勿混淆。
- 构建遗留物：`0Web_updated.zip`、`build_launcher_*.log`、`build/`。
- 待办验收（见 PROJECT_RECORD §7/§11/§13）：迁移 002 实机确认、`sensor_minute_aggregates` 每分钟出数、MQTT→MySQL 端到端、倾倒实测、告警解除。
- 下一阶段（计划，未开始）：积累 1 分钟粒度数据 → 数据导出/质量检查 → 箱内温湿度 30 min / 1 h 预测模型（ML 代码在 `F:\0GateWay\ml/`，现为空）。
