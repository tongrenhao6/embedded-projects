# 节点在线状态实现计划

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 根据节点 telemetry 的最后接收时间，在 API 和网页上显示节点在线、通信不稳定或离线状态。

**架构：** `TelemetryState` 保留内存中的最新接收时间；API 对历史节点使用分钟聚合或旧原始表的最后接收时间作为回退。统一的纯函数按 10 秒和 60 秒阈值计算 `online`、`stale`、`offline`、`unknown`，网页通过现有 2 秒轮询刷新节点状态。

**技术栈：** Python、FastAPI、MySQL、原生 JavaScript、Pytest。

---

### 任务 1：节点新鲜度判定

**文件：**
- 创建：`beehive_gateway/node_presence.py`
- 测试：`tests/test_node_presence.py`

- [ ] 编写测试，覆盖无最后接收时间为 `unknown`、5 秒为 `online`、30 秒为 `stale`、61 秒为 `offline`，以及时钟略微倒退时年龄不小于 0。
- [ ] 运行 `python -m pytest tests/test_node_presence.py -q`，确认新测试因模块或函数不存在而失败。
- [ ] 实现 `get_node_presence(last_seen_at, now=None)`，返回 `status`、`last_seen_at` 和 `last_seen_age_seconds`，阈值常量固定为 10 和 60 秒。
- [ ] 重新运行该测试，确认全部通过。

### 任务 2：API 返回节点状态

**文件：**
- 修改：`beehive_gateway/telemetry_state.py`
- 修改：`beehive_gateway/api.py`
- 修改：`beehive_gateway/storage.py`
- 测试：`tests/test_api.py`

- [ ] 为 `TelemetryState` 增加读取节点最新快照的接口使用现有线程锁。
- [ ] 扩展 MySQL `list_nodes`，返回数据库中每个节点的最后 `received_at`；分钟聚合数据优先，旧 `sensor_readings` 作为兼容回退。
- [ ] 修改 `/api/gateways/{gateway_id}/nodes`，合并内存、分钟表和旧表节点，并对每个节点调用统一新鲜度函数。
- [ ] 增加 API 测试，验证内存最新时间优先、历史时间可回退，以及在线、stale、offline 字段。
- [ ] 运行 `python -m pytest tests/test_api.py -q`，确认通过。

### 任务 3：网页显示节点状态

**文件：**
- 修改：`beehive_gateway/web/index.html`
- 修改：`beehive_gateway/web/app.js`
- 修改：`beehive_gateway/web/styles.css`
- 测试：`tests/test_api.py`

- [ ] 增加节点状态徽章和“节点状态 / 最后上报”诊断项。
- [ ] 节点下拉框显示节点编号及状态，选中节点时显示最后上报时间和年龄。
- [ ] 将 `online`、`stale`、`offline`、`unknown` 映射为“节点在线”“通信不稳定”“节点离线”“暂无节点数据”，并保留最后测量值。
- [ ] 增加静态资源测试断言，确保前端包含节点状态字段和阈值文案。
- [ ] 运行 `node --check beehive_gateway/web/app.js` 和相关 Pytest。

### 任务 4：文档与完整验证

**文件：**
- 修改：`docs/PROJECT_RECORD.md`
- 修改：`README.md`

- [ ] 记录状态判定依据、阈值和“只能判断网关是否收到节点数据”的边界。
- [ ] 运行 `python -m pytest`、`python -m compileall -q beehive_gateway tests`、`node --check beehive_gateway/web/app.js`。
- [ ] 汇报 API 字段、页面表现和验证结果；不重新打包或烧录 ESP32，除非用户另行要求。
