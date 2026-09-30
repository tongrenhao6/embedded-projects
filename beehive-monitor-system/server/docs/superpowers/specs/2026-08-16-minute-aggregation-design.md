# 分钟聚合与训练数据设计

## 目标

每个节点在内存中保留最新 2 秒遥测用于实时展示和倾倒告警；仅将每分钟一条的聚合数据写入 MySQL，供 Web 历史展示和时序预测模型训练使用。

## 范围

- 新增 MySQL 分钟聚合表及迁移。
- 每一条成功接收的 MQTT telemetry 更新内存中的最新状态和当前分钟累计值。
- 分钟结束后，仅写入一条聚合记录。
- 提供分钟聚合历史查询 API；原始历史 API 保持兼容。
- Web 历史曲线改用分钟聚合数据；实时卡片继续读取原始最新数据。
- 在 `docs/PROJECT_RECORD.md` 维护可用于论文的系统事实和决策。

不在本阶段训练或部署预测模型，不修改节点采样周期，也不修改网关 MQTT 协议。已有 `sensor_readings` 表中的历史原始记录不删除，但改造完成后不再写入该表。

## 数据模型

新增表 `sensor_minute_aggregates`，主键为 `(gateway_id, node_id, bucket_start)`。

- `bucket_start`：按数据库接收时间向下取整的分钟，`DATETIME(0)`。
- `sample_count`：该分钟收到的总有效 telemetry 数量。
- 每个温湿度字段都保存 `avg`、`min`、`max` 和 `valid_count`；输入为 `NULL` 时不修改该字段的统计值。
- `tilt_valid_count`：姿态字段非空的样本数。
- `tilt_alarm_count`：姿态为真或状态位 bit 3 已触发的样本数。
- `status_flags_or`：该分钟所有状态位的按位或，用于保留发生过的有效性和告警信息。
- `last_sequence_no`、`last_uptime_ms`、`last_received_at`：该分钟末条样本的可追溯信息。

聚合平均值以增量均值公式维护：对非空新值 `x`，新平均值为 `(old_avg * old_count + x) / (old_count + 1)`。因此不需要长期保存额外的求和列。

## 实时缓存、写入与一致性

每个节点维护一个内存状态：最新 telemetry 和当前分钟的累计聚合器。MQTT worker 按顺序处理消息，处理顺序如下：

1. 校验 telemetry 后立即更新节点最新状态缓存，供 `latest` API 使用。
2. 将非重复 telemetry 纳入当前分钟的内存聚合器；`NULL` 测量值不计入对应统计值。
3. 检测到新分钟时，将前一分钟聚合器写入 `sensor_minute_aggregates`，然后创建新一分钟聚合器。
4. 服务正常退出时尽力刷写当前未结束分钟；异常掉电或崩溃最多丢失当前一分钟的未落库统计，不影响已经提交的分钟数据。
5. 网关 online/offline 状态仍写入 `gateways`。

分钟归属使用 Web 服务接收时间，不使用节点 `uptime_ms`，以保证多节点、重启和时间漂移场景下的统一时间轴。缓存大小固定为“每节点一个最新状态加一个当前分钟聚合器”，不随运行时长增长。

## 查询与 Web 行为

- `GET /api/gateways/{gateway_id}/nodes/{node_id}/latest` 优先返回内存中的最新 2 秒数据；服务刚启动且尚未收到 telemetry 时，回退为最近一分钟聚合数据。
- 新 API：`GET /api/gateways/{gateway_id}/nodes/{node_id}/minute-history?limit=1440`。
- `limit` 允许 1 至 10,080，分别覆盖最小查询和最长 7 天的一分钟历史。
- 返回按接收分钟倒序排列，字段使用 `inside_temperature_avg`、`inside_humidity_avg`、`outside_temperature_avg`、`outside_humidity_avg`、倾倒统计和样本数。
- 监控页实时卡片继续调用 `latest`，历史曲线与历史表切换为 `minute-history`；页面清楚显示数据粒度为“分钟聚合”。

## 历史记录边界

- 分钟聚合表长期保留，作为论文统计和模型训练集。
- 新 telemetry 不再写入 `sensor_readings`，因此不需要新增原始数据定期清理命令。
- 已有的 `sensor_readings` 数据保留不动，不自动回填为分钟聚合数据。

## 测试与验收

- 每条 telemetry 立即更新内存最新状态；一分钟内的多条 telemetry 仅生成一条聚合数据库记录。
- 重复序号不改变最新状态缓存或分钟聚合值。
- 多条非空测量值正确生成均值、最小值、最大值和有效样本数。
- `NULL` 测量值不会被解释为零，也不会污染对应统计值。
- `tilted=true`、`tilted=1` 或状态位 bit 3 触发时，倾倒计数正确增加。
- 查询范围、排序和节点/网关隔离正确。
- Web 服务重启后，在收到首条新 telemetry 前，`latest` API 回退至最后一条分钟聚合记录。
- 完整 Python 测试、MySQL 迁移检查和 Web 静态资源检查通过。
