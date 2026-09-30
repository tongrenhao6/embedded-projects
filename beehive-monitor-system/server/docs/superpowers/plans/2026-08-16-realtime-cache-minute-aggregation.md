# 实时缓存与分钟聚合实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 保持 MQTT telemetry 每 2 秒到达并实时显示，但每个网关节点每分钟只向 MySQL 写入一条聚合记录。

**架构：** MQTT worker 收到并校验消息后，更新线程安全的每节点最新值缓存，并把当前分钟的有效样本交给内存聚合器。分钟切换时，聚合器把上一分钟写入 `sensor_minute_aggregates`；`latest` API 优先读取缓存，历史 API 读取分钟表。已有 `sensor_readings` 历史数据保留，但新 telemetry 不再写入该表。

**技术栈：** Python 3.11、FastAPI、Paho MQTT、MySQL 8、原生 JavaScript/CSS、pytest。

---

## 文件结构

- 创建：`F:\0Web\db\migrations\002_add_minute_aggregates.sql`，新增分钟聚合表。
- 创建：`F:\0Web\beehive_gateway\telemetry_state.py`，线程安全的最新值缓存、分钟累计器和分钟切换逻辑。
- 修改：`F:\0Web\beehive_gateway\storage.py`，写入分钟聚合、读取最新聚合和分钟历史。
- 修改：`F:\0Web\beehive_gateway\mqtt_ingest.py`，把有效 telemetry 交给状态协调器，不再插入 `sensor_readings`。
- 修改：`F:\0Web\beehive_gateway\api.py`，注入状态缓存并增加分钟历史接口，`latest` 优先返回缓存。
- 修改：`F:\0Web\beehive_gateway\web\app.js`、`index.html`，实时轮询最新数据并使用分钟历史曲线。
- 修改：`F:\0Web\tests\test_storage.py`、`test_mqtt_ingest.py`，覆盖分钟写入和重复消息行为。
- 创建：`F:\0Web\tests\test_telemetry_state.py`，覆盖缓存、聚合、空值和分钟切换。
- 修改：`F:\0Web\tests\test_api.py`，覆盖缓存优先和分钟历史 API。
- 修改：`F:\0Web\docs\PROJECT_RECORD.md`，完成后记录实际实现与验证结果。

### 任务 1：写数据库迁移和状态单元测试

**文件：**
- 创建：`db/migrations/002_add_minute_aggregates.sql`
- 创建：`tests/test_telemetry_state.py`

- [ ] **步骤 1：编写失败测试**

测试必须证明：30 条样本在同一分钟内只产生一个累计器；温湿度均值、最小值、最大值和有效计数正确；`None` 不计为 0；`tilted=1` 或状态位 bit 3 会增加倾倒计数；进入新分钟时返回上一分钟聚合并开始新桶。

```python
def test_minute_accumulator_flushes_on_bucket_change():
    state = TelemetryState()
    first = reading(inside_temperature=25.0, status_flags=7, tilted=False)
    alarm = reading(inside_temperature=None, status_flags=15, tilted=True)

    assert state.ingest("gw-001", first, datetime(2026, 8, 16, 12, 0, 1)) is None
    aggregate = state.ingest("gw-001", alarm, datetime(2026, 8, 16, 12, 1, 0))

    assert aggregate.inside_temperature_avg == Decimal("25.00")
    assert aggregate.inside_temperature_valid_count == 1
    assert aggregate.sample_count == 1
    assert aggregate.tilt_alarm_count == 0
```

- [ ] **步骤 2：运行测试验证失败**

运行：`python -m pytest tests/test_telemetry_state.py -q`

预期：FAIL，报错 `TelemetryState` 尚未定义。

- [ ] **步骤 3：编写迁移和最小状态接口**

迁移创建 `sensor_minute_aggregates`，主键为 `(gateway_id, node_id, bucket_start)`，包含 `sample_count`、四组温湿度 `avg/min/max/valid_count`、`tilt_valid_count`、`tilt_alarm_count`、`status_flags_or`、`last_sequence_no`、`last_uptime_ms` 和 `last_received_at`。状态模块提供：

```python
class TelemetryState:
    def ingest(self, gateway_id: str, reading: SensorReading, received_at: datetime) -> MinuteAggregate | None: ...
    def latest(self, gateway_id: str, node_id: int) -> SensorReading | None: ...
    def flush(self, now: datetime | None = None) -> list[MinuteAggregate]: ...
```

- [ ] **步骤 4：运行测试验证通过**

运行：`python -m pytest tests/test_telemetry_state.py -q`

预期：状态缓存测试全部 PASS。

- [ ] **步骤 5：Commit**

当前目录不是 Git 仓库，因此保存文件并记录变更，不执行虚构的 commit 命令。

### 任务 2：实现 MySQL 聚合存储

**文件：**
- 修改：`beehive_gateway/storage.py`
- 修改：`tests/test_storage.py`

- [ ] **步骤 1：编写失败测试**

使用现有假连接验证 `store_minute_aggregate()` 传递参数化 SQL、提交事务，并验证 `minute_history()` 的节点和网关条件以及 `limit` 范围。

- [ ] **步骤 2：运行测试验证失败**

运行：`python -m pytest tests/test_storage.py -q`

预期：FAIL，报错新存储方法尚未定义。

- [ ] **步骤 3：实现最小存储方法**

新增参数化 UPSERT：

```sql
INSERT INTO sensor_minute_aggregates (...)
VALUES (...)
ON DUPLICATE KEY UPDATE
  sample_count = VALUES(sample_count),
  status_flags_or = VALUES(status_flags_or),
  last_sequence_no = VALUES(last_sequence_no),
  last_received_at = VALUES(last_received_at)
```

同时新增 `latest_minute_aggregate()` 和 `minute_history(gateway_id, node_id, limit)`，所有查询继续使用参数化占位符。

- [ ] **步骤 4：运行测试验证通过**

运行：`python -m pytest tests/test_storage.py -q`

预期：存储测试 PASS。

### 任务 3：接入 MQTT worker 和幂等边界

**文件：**
- 修改：`beehive_gateway/mqtt_ingest.py`
- 修改：`tests/test_mqtt_ingest.py`

- [ ] **步骤 1：编写失败测试**

测试有效 telemetry 更新状态缓存但不调用旧的原始插入方法；同一进程收到相同 `gateway_id/node_id/sequence` 两次时，只计入一次；状态消息仍更新网关状态；Broker 断线仍不退出 worker。

- [ ] **步骤 2：运行测试验证失败**

运行：`python -m pytest tests/test_mqtt_ingest.py -q`

预期：FAIL，现有 worker 仍调用原始 `store_gateway_reading()`。

- [ ] **步骤 3：实现协调流程**

在 `MqttReceiver` 中持有 `TelemetryState`，消息校验成功后调用 `state.ingest()`；只有收到上一分钟的 `MinuteAggregate` 时才调用 `storage.store_minute_aggregate()`。重复序号在当前进程由状态层去重，网关状态消息继续走 `update_gateway_status()`。服务停止时调用 `flush()` 尽力刷写未结束分钟。

- [ ] **步骤 4：运行测试验证通过**

运行：`python -m pytest tests/test_mqtt_ingest.py tests/test_telemetry_state.py -q`

预期：全部 PASS，MQTT 连接异常测试仍确认 HTTP 不退出。

### 任务 4：更新 API 和 Web 实时/历史数据流

**文件：**
- 修改：`beehive_gateway/api.py`
- 修改：`tests/test_api.py`
- 修改：`beehive_gateway/web/app.js`
- 修改：`beehive_gateway/web/index.html`

- [ ] **步骤 1：编写失败测试**

扩展 `FakeStorage` 和 API 测试：缓存有数据时 `latest` 返回缓存值；缓存为空时返回最近分钟聚合；`minute-history` 返回分钟字段并拒绝超过 `10080` 的 limit。

- [ ] **步骤 2：运行测试验证失败**

运行：`python -m pytest tests/test_api.py -q`

预期：FAIL，分钟接口尚未注册。

- [ ] **步骤 3：实现最小 API 和页面接入**

新增：

```text
GET /api/gateways/{gateway_id}/nodes/{node_id}/minute-history?limit=1440
```

`latest` 依赖注入 `TelemetryState` 后优先查缓存，缓存为空回退 `latest_minute_aggregate()`。页面实时数据轮询周期调整为 2 秒，历史图表调用 `minute-history`，并显示“分钟聚合”粒度；倾倒告警仍依据最新 telemetry 即时显示。

- [ ] **步骤 4：运行测试验证通过**

运行：`python -m pytest tests/test_api.py -q` 和 `node --check beehive_gateway/web/app.js`

预期：API 测试和 JavaScript 语法检查 PASS。

### 任务 5：全量验证和项目记录

**文件：**
- 修改：`docs/PROJECT_RECORD.md`
- 修改：`README.md`

- [ ] **步骤 1：运行完整验证**

运行：

```powershell
python -m compileall -q beehive_gateway tests
python -m pytest
node --check beehive_gateway\web\app.js
```

预期：所有测试通过、编译无错误、JavaScript 语法检查退出码为 0。

- [ ] **步骤 2：验证数据库迁移**

在 MySQL 测试数据库执行 `db/migrations/002_add_minute_aggregates.sql`，检查表结构、主键和索引；不在生产库执行破坏性操作。

- [ ] **步骤 3：验证实时链路**

启动 `beehive-web`，观察页面每约 2 秒更新最新温湿度，等待分钟切换后查询 `minute-history` 确认新增一行，并确认 MySQL 不新增 2 秒原始行。

- [ ] **步骤 4：更新项目记录**

只把已经由测试或实机验证的行为写入 `docs/PROJECT_RECORD.md`，明确记录部署日期、数据粒度、测试结果和任何已知限制。

