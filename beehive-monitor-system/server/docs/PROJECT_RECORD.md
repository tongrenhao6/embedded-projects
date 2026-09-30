# 蜂箱监测系统项目记录

本文档记录已经确认的系统事实、关键设计决策和验证结果。它面向后续开发、论文撰写和答辩材料整理；不记录数据库密码、Wi-Fi 密码或其他私密凭据。

## 1. 项目目标

蜂箱监测系统由低功耗传感器节点、ESP32-S3 网关、MQTT 数据链路、MySQL 数据库和 Web 监控页面组成。当前阶段的目标是可靠采集箱内外温湿度和倾倒状态，支持实时展示、历史查询与后续时序预测。

## 2. 当前架构

```text
SHT30 x2 + MPU6050
        |
ESP32-S3 节点 -- LoRa UART --> ESP32-S3 网关 -- Wi-Fi/MQTT --> PC Web 服务
                                                                  |
                                                                MySQL
                                                                  |
                                                          FastAPI + 浏览器页面
```

- 节点与网关板卡：ESP32-S3-WROOM-1 N16R8。
- LoRa 模块：DX-LR22-433T22D，433 MHz UART 透明传输模块。
- 网关 LoRa 接线：UART1，GPIO4 为 TX、GPIO5 为 RX，9600 bps、8N1、无校验、无硬件流控。
- 网关 ID：`gw-001`。
- 节点帧由网关解码后，以 MQTT JSON telemetry 上报。
- Web 服务运行于 `F:\0Web`，提供 MQTT 接收、MySQL 存储、查询 API 和监控页面。

## 3. 节点遥测协议

节点到网关采用固定 26 字节二进制帧：

- Magic：`0xB1`；协议版本：`1`；消息类型：`1`；Payload length：`20`。
- 多字节字段为大端序。
- 校验：CRC-16/CCITT-FALSE，poly `0x1021`，init `0xFFFF`，无反射、无 final XOR。
- 字段包括节点 ID、序号、运行时间、箱内外温湿度、状态位和 CRC。
- `status_flags`：bit 0 为箱内有效，bit 1 为箱外有效，bit 2 为倾倒数据有效，bit 3 为当前倾倒。
- 当对应有效位未置位时，对应测量值必须视为无效。

网关已实现串口噪声跳过、固定帧同步、拆包恢复、CRC 校验和错误统计。节点帧的 LoRa 上报周期当前为 2 秒。

## 4. MQTT 与数据安全边界

- 开发联调 Broker：`mqtt://broker.emqx.io:1883`，无 TLS，仅用于开发和答辩联调。
- Topic prefix：`beehive/v1`。
- telemetry 订阅：`beehive/v1/gateways/+/nodes/+/telemetry`。
- 网关状态订阅：`beehive/v1/gateways/+/status`。
- telemetry 使用 QoS 1、非 retained；网关状态使用 QoS 1、retained，并通过 LWT 报告离线。
- Web 端将 MQTT 输入视为不可信数据，校验主题、JSON、数值范围、节点 ID、状态字段和网关 ID。
- 任何密码只通过环境变量提供，不写入源码、本文档或 Git。

## 5. MySQL 数据事实

- 数据库：`beehive_gateway`。
- 原始遥测表：`sensor_readings`。
- 网关状态表：`gateways`。
- 当前实现中，原始 telemetry 每次校验成功后写入 `sensor_readings`。这是分钟聚合改造前的行为。
- 唯一键为 `(gateway_id, node_id, sequence_no)`；MQTT QoS 1 的重投不会产生重复记录。
- 状态消息会更新 `gateways.status` 与最后上报时间。
- MySQL 的 `BOOLEAN` 经 API 返回可能表现为 `0/1`；Web 页面同时兼容 `false/true` 和 `0/1`。

## 6. 已验证行为

- 网关已实机接收 LoRa 节点数据并完成 MQTT 上报。
- Web API 可读取网关、节点、最新遥测和历史遥测。
- 倾倒状态：当 `status_flags=0x000F` 且 telemetry 返回 `tilted=1` 时，网页显示“已倾倒（告警中）”。
- RGB 灯已关闭，避免网关工作期间持续点亮。

## 7. 已批准的数据与模型计划（待数据库迁移和实机验证）

节点每 2 秒发送一条遥测，约为 43,200 条/天/节点。该频率用于实时倾倒告警是合理的，但温湿度预测不需要保存同等密度的长期训练数据。

已确认后续采用“实时缓存 + 分钟聚合入库”策略：

1. 实时缓存：每个节点仅保留最新一条 2 秒 telemetry，供 Web 页面实时显示和倾倒告警。缓存不累计历史数据，内存占用仅随节点数增加。
2. 分钟聚合：内存中累计当前分钟的温湿度均值、最小值、最大值、有效样本数、倾倒次数和状态位；每分钟仅向 MySQL 写入一条聚合记录。
3. 长期存储：改造完成后不再写入原始 2 秒 telemetry。已有 `sensor_readings` 历史记录保留不动，但不作为新模型训练集。

模型的第一阶段目标为“未来 30 分钟或 1 小时的箱内温湿度预测”，不将疾病、分蜂等没有人工标签的复杂事件作为首个模型目标。实时缓存、分钟聚合写入、分钟历史 API 和网页 2 秒刷新代码已经完成，等待执行 MySQL 迁移 `db/migrations/002_add_minute_aggregates.sql` 并重启服务后进行实机验证。实施细节见 `docs/superpowers/specs/2026-08-16-minute-aggregation-design.md`。

代码层已通过 51 项主机测试。为避免公共 MQTT Broker 的非预期主题组合耗尽内存，实时状态最多跟踪 256 个网关/节点组合；完成分钟但尚未写入 MySQL 的聚合记录使用有界待写队列重试。上述容量边界属于保护机制，实机部署后应继续监测日志中的容量或存储错误。

## 8. 论文可引用指标与计划边界

后续论文可基于以下可核验指标撰写：

- 节点采样和上报周期：2 秒。
- 节点到网关使用 UART 透明 LoRa；网关到服务端使用 Wi-Fi 与 MQTT。
- 端到端数据可靠性措施：二进制帧 CRC、串口流恢复、MQTT QoS 1、数据库唯一键幂等写入。
- 数据有效性措施：传感器状态位控制 `null` 值、输入范围校验、无效值不参与聚合和模型训练。
- 计划仅保存 1 分钟粒度训练数据，减少连续高相关采样带来的冗余；在聚合功能完成并积累数据前，不应写成已完成实验。

## 9. 维护规则

以后每完成一项影响协议、硬件接线、数据表、MQTT、模型、部署方式或实机验证结果的重要工作，都在本文档追加对应事实、日期和结论。未验证的设想必须明确标注为“计划”，不得写成已完成事实。

## 10. Windows 一键启动程序

### 已实现并验证

- 启动入口：`beehive_gateway/launcher.py`；构建脚本：`scripts/build_launcher.ps1`。
- 已在 2026-08-16 使用 PyInstaller 成功生成 `dist/BeehiveMonitor.exe`，并同时生成 `dist/BeehiveMonitor.env.example`。
- EXE 从自身同目录的 `.env` 读取运行配置，且不会覆盖系统中已有的同名环境变量。
- `.env` 不会被打入 EXE。首次仅需由部署者填写 `BEEHIVE_DB_PASSWORD`；数据库、MQTT 与 HTTP 的其余默认值已在示例文件中给出。
- 已验证未配置 `.env` 时，EXE 会明确提示缺少 `BEEHIVE_DB_PASSWORD`，给出 `.env` 的准确位置并以退出码 `2` 结束，不会静默失败。
- 构建脚本已排除 Anaconda 环境中的 `PyQt5` 和 `PySide6`，消除了二者同时被 PyInstaller 收集时的构建冲突；PyInstaller 失败时脚本会立即终止。
- 因 `uvicorn` 和 `mysql-connector` 使用动态导入，构建脚本显式收集 `beehive_gateway.api` 与 `mysql.connector.locales.eng.client_error`；否则 EXE 会分别报 ASGI 模块缺失或 MySQL 英文错误码本地化缺失。

### 使用边界与待实机确认事项

- 首次部署需将 `BeehiveMonitor.env.example` 复制为 `.env`，并填写 `beehive_app` 的数据库密码；该文件不得提交或分享。
- 启动前必须停止已有的 `beehive-web` 进程，否则 HTTP 端口 `127.0.0.1:8000` 会冲突。
- EXE 已完成构建、无配置启动路径验证和临时数据库端口启动验证；后一次验证已进入 `beehive_gateway.api` 的 MySQL 连接阶段，未再出现模块导入错误。填写实际数据库密码后的“自动启动服务并打开网页”仍需在用户本机重新双击确认。
- 当前 EXE 约为 208 MB。这是因为打包时使用了完整 Anaconda 环境；功能不受影响。若后续需要分发，再建立只安装运行依赖的独立虚拟环境重新打包，以减小体积。
- 2026-08-19：用户已完成首次 `.env` 配置，并确认双击 `BeehiveMonitor.exe` 可以成功启动服务。MQTT 到 MySQL 的端到端数据写入仍需按下方验收步骤确认。

## 11. 当前验收与下一阶段

## 12. 节点在线状态推断（2026-08-24）

节点当前未单独发布 online/offline MQTT 状态。Web 端依据网关最后收到该节点 telemetry 的时间推断通信新鲜度：最近 10 秒内为 `online`，超过 10 秒且不超过 60 秒为 `stale`（通信不稳定），超过 60 秒为 `offline`，没有历史接收时间为 `unknown`。

`GET /api/gateways/{gateway_id}/nodes` 和节点 `latest` 接口返回 `status`、`last_seen_at`、`last_seen_age_seconds`。内存实时快照优先，服务重启后回退到分钟聚合的 `last_received_at`，再回退到原始遥测表的 `received_at`。页面每 2 秒刷新状态；节点停止上报时保留最后一次测量值并标记为过期。

该状态只能证明“网关最近是否收到节点数据”，不能单独区分节点断电、节点故障和 LoRa 无线链路中断。后续若需要更精确诊断，可在节点增加心跳或在网关增加 LoRa 链路统计。

## 13. 巴法云 MQTT 主题适配（2026-09-02）

巴法云自定义主题只允许字母和数字，因此增加 `bemfa` 平面主题模式。当前主题为 `beehivegw001node001telemetry`（节点遥测）和 `beehivegw001status`（网关状态），网关 ID 为 `gw-001`、节点编号为 `1`。网关与 Web 服务通过配置项切换主题模式，数据库、页面和数据协议保持不变。新增节点时需在巴法云创建对应主题，并增加相应接收配置。

### 启动后验收顺序

1. 确认网关和节点保持供电，浏览器打开 `http://127.0.0.1:8000/`。
2. 检查页面能显示网关在线、节点最新温湿度和倾倒状态。
3. 使用 MySQL 查询确认 `sensor_minute_aggregates` 在新的整分钟产生记录。
4. 使用 `/api/.../latest` 和 `/api/.../minute-history` 确认实时缓存与分钟历史接口均有数据。
5. 实际倾斜节点，确认状态从 `tilted=false` 变为 `tilted=true`，恢复后告警解除。

### 下一阶段目标（计划）

- 连续积累稳定的 1 分钟粒度数据，暂不急于训练预测模型。
- ~~增加数据导出与数据质量检查，为后续特征工程和预测模型准备训练集。~~ 已于 2026-09-08 实现接口与页面入口（见 §14）。
- 首个模型目标仍为未来 30 分钟或 1 小时的箱内温湿度预测。

## 14. 数据导出与质量检查（2026-09-08，代码完成）

- 分钟聚合 CSV 导出接口 `GET /api/gateways/{gateway_id}/nodes/{node_id}/minute-history/export` 已实现：默认导出近 168 小时，可用 `hours`、`start`、`end` 调整窗口。CSV 为 UTF-8 BOM 编码，列与 `sensor_minute_aggregates` 表一致，空缺传感器值输出空单元格，可直接被 pandas / Excel 读取，作为后续温湿度预测模型的训练数据来源。页面明细区新增“导出 CSV”入口，链接跟随当前网关/节点选择。
- 数据质量接口 `GET /api/gateways/{gateway_id}/nodes/{node_id}/quality?hours=24`（1–720 小时）已实现：返回窗口内分钟行数、按首末桶时间推算的期望行数、缺失分钟数与占比、四路温湿度有效率（基于有效样本计数 / 总样本数）以及倾倒告警次数与占比；窗口内无数据时仅返回 `minute_rows: 0`。页面明细区新增“质量统计”入口（新窗口打开 JSON）。
- 顺带修复：页面头部自动刷新文案从 5s 修正为 2s，与实际轮询周期一致（该不一致最早可追溯到分钟聚合改造时期）。
- 代码层已通过 74 项主机测试（含本次新增 11 项）。
- **实机验收（2026-09-08 20:24 完成）**：`BeehiveMonitor.exe` 已用新代码重建并重启，实测通过——导出 CSV 接口返回带 BOM 的正确 CSV；quality 接口在 720 小时窗口返回 215 个分钟行。首次实机验证时发现并修复了一个时区缺陷：接口原用 UTC 构造时间窗口，与本地时间存储的 `bucket_start` 相差 8 小时，导致 24 小时窗口漏查当天数据；修复后默认 24 小时窗口能查到当天分钟行。
- **端到端确认（补充 §7/§11 的待办）**：分钟聚合到 MySQL 的链路工作正常——2026-09-08 19:12 至 20:12 期间 `sensor_minute_aggregates` 新增 32 行（网关 MQTT 连上后即开始落库）。此前“09-02 后无新数据”的判断是 UTC 时区缺陷造成的误读，不是数据链路故障。历史数据共 215 行：08-16 至 09-02 为 183 行，样本有效率（箱内外温湿度）均为 100%，平均约 27.6 样本/分钟（理论 30，存在少量 LoRa 丢帧）。
- **倾倒问题的量化证据**：720 小时窗口内 `tilt_valid_samples` 为 5053，比 `total_samples` 5944 少 891——缺的正是 09-08 当天的全部样本，与节点端 MPU6050 无效（`status_flags=0x03`）的现象完全吻合，进一步确认问题在节点侧而非网关/网页。
- **测试数据清空（2026-09-10）**：经确认全部为测试数据后，三张表已清空（分钟表 228 行、原始表 244 行、网关表 1 行），清空前备份在 `db\backups\*_20260910_095210.csv`。自 2026-09-10 起 `sensor_minute_aggregates` 从零积累正式数据，未来导出的精简 CSV（`format=lean`，7 列）即为候选训练集；清库可用 `scripts/clear_test_data.py`（自动先备份）。
