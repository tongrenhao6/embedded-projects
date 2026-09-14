# ph-water-server

实验第一版说明文档

## 版本定位

这是 pH 水质监测系统的实验第一版网页与服务端项目。

这一版的目标不是做完整产品，而是先把下面这条链路打通并稳定下来：

1. ESP32 通过 MQTT 上报实时数据和补传数据
2. Node.js 服务端接收数据并写入 MySQL
3. 网页端登录后查看实时数据、历史数据、补传状态
4. 管理员在网页端下发补传命令，触发 ESP32 重放指定月份 CSV 数据
5. 服务端按 `device_id + sample_time` 幂等写入，避免补传时重复插入

当前项目路径：

```text
E:\ph-water-server
```

## 技术栈

- Node.js
- Express 5
- MySQL 8
- MQTT
- 原生 HTML / CSS / JavaScript

## 当前功能范围

本版本已经实现：

- 登录鉴权
- 普通用户和管理员角色区分
- 采样数据查询
- 采样数据分页
- pH / 温度趋势图
- MQTT 在线状态展示
- 设备未上传状态展示
- 未上传数据弹窗提醒
- 管理员网页端下发补传命令
- ESP32 命令执行状态回执展示
- 指定月份强制补传
- 样本按 `device_id + sample_time` 幂等去重

本版本明确仍属于实验性质：

- Session 保存在服务端内存中，重启服务会丢失登录会话
- 默认只围绕一个设备示例 `ph_terminal_001`
- 页面和接口以单机部署、内网调试为主
- 没有做生产级审计、限流、告警、HA、备份自动化

## 项目结构

```text
.
├─ public/                前端页面与脚本
├─ routes/                HTTP 路由
├─ services/              业务逻辑
├─ middleware/            鉴权中间件
├─ sql/                   初始化和升级 SQL
├─ db.js                  MySQL 连接
├─ mqtt-client.js         MQTT 订阅与命令下发
├─ server.js              Express 入口
└─ package.json
```

## 环境变量

参考 [.env.example](E:/ph-water-server/.env.example)：

```env
PORT=3000
DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=root
DB_PASSWORD=change_me
DB_NAME=water_monitor
MQTT_URL=mqtt://120.27.235.176:1883
MQTT_USERNAME=root
MQTT_PASSWORD=change_me
MQTT_BASE_TOPIC=phwater/v1
MQTT_CLIENT_ID=ph_water_server
```

说明：

- `PORT`：网页服务端口
- `DB_*`：MySQL 连接配置
- `MQTT_*`：ESP32 所连接的 MQTT Broker 配置
- `MQTT_BASE_TOPIC`：设备 topic 前缀，当前约定为 `phwater/v1`

## 数据库初始化

新库初始化：

```sql
sql/init.sql
```

旧库升级补传状态和去重索引：

```sql
sql/upgrade-upload-status.sql
```

当前数据库核心表：

- `devices`：设备信息
- `sensors`：传感器信息
- `samples`：采样主表
- `sample_values`：扩展指标值
- `commands`：命令队列与执行结果
- `upload_logs`：上传日志预留表
- `device_upload_status`：设备未上传状态
- `users`：登录用户

### 幂等约束

`samples` 表已按下面字段做唯一约束：

```text
device_id + sample_time
```

这样 ESP32 实时上报、断线重试、补传重放时，如果同一设备同一采样时间再次到达，服务端会更新已有记录，而不是重复插入。

## 默认账号

初始化 SQL 会写入两个账号：

- 管理员：`admin`，密码：admin123
- 普通用户：`user`，密码：user123


密码哈希已经写在 `sql/init.sql` 中。

如果需要重置普通用户密码，可以通过登录页的“忘记密码”入口处理；管理员也可以在用户管理页进行维护。

## 启动方式

安装依赖：

```bash
npm install
```

启动服务：

```bash
npm run start
```

开发时也可使用：

```bash
npm run dev
```

启动后访问：

```text
http://127.0.0.1:3000
```

未登录访问 `/` 时会自动跳转到 `/login.html`。

## 登录与权限

当前权限模型分两种：

- `user`：普通用户，只能查看数据和个人资料
- `admin`：管理员，可以查看数据、下发命令、管理用户

服务端中间件位置：

- [middleware/auth.js](E:/ph-water-server/middleware/auth.js)

当前会话机制：

- 使用服务端内存 `Map` 保存 session
- 通过 `HttpOnly` Cookie `ph_session` 维持登录
- Session 有效期 8 小时
- 服务端重启后 session 会失效，需要重新登录

## 前端页面说明

登录页：

- [public/login.html](E:/ph-water-server/public/login.html)

主页：

- [public/index.html](E:/ph-water-server/public/index.html)

实验第一版网页包含以下页面区域：

- 实时数据概览
- 趋势图
- 历史数据查询
- 个人资料
- 远程命令（管理员）
- 用户管理（管理员）

### 首页重点展示

- 最近 pH
- 最近温度
- 最近采样时间
- 数据来源 `source`
- 未上传数量 `pending_count`
- 失败数量 `failed_count`
- 最早未上传时间 `oldest_sample_time`
- 最近错误 `last_error`

### 未上传弹窗

当设备存在未上传或失败数据时，网页会弹窗提醒：

- 管理员可点击“立即补传”
- 任意用户可点击“稍后提醒”

当前逻辑：

- 本轮状态未变化时，点击“稍后提醒”后不重复弹
- 当未上传状态变化或页面重新加载后，可再次提醒

## HTTP API

### 认证

- `POST /api/auth/login`
- `GET /api/auth/me`
- `PATCH /api/auth/me`
- `POST /api/auth/forgot-password`
- `POST /api/auth/logout`

### 数据查询

- `GET /api/samples`

支持参数：

- `device_id`
- `date`
- `start_date`
- `end_date`
- `limit`
- `page`

### 未上传状态

- `GET /api/upload-status?device_id=ph_terminal_001`

返回示例：

```json
{
  "ok": true,
  "status": {
    "device_id": "ph_terminal_001",
    "pending_count": 12,
    "failed_count": 2,
    "oldest_sample_time": "2026-05-14 08:30:00",
    "last_upload_at": "2026-05-14 10:20:00",
    "last_error": "PUB_TIMEOUT",
    "updated_at": "2026-05-14 10:21:00"
  }
}
```

### 命令

- `POST /api/commands`
- `GET /api/commands`
- `GET /api/commands/:id`

说明：

- 当前命令接口仅管理员可用
- 下发命令后，服务端会写入 `commands` 表
- MQTT 发布成功后，命令状态会从 `pending` 进入 `running`
- ESP32 再通过 `commands/up` 回传最终结果

## MQTT 约定

基础 topic：

```text
phwater/v1
```

单设备 topic 结构：

```text
phwater/v1/devices/{deviceId}/telemetry
phwater/v1/devices/{deviceId}/status
phwater/v1/devices/{deviceId}/commands/down
phwater/v1/devices/{deviceId}/commands/up
phwater/v1/devices/{deviceId}/upload/status
```

### telemetry

ESP32 上报实时数据或补传数据。

示例：

```json
{
  "device_id": "ph_terminal_001",
  "sample_time": "2026-05-14 08:30:00",
  "ph": 7.12,
  "temperature": 25.4,
  "source": "realtime",
  "readings": {
    "ph": { "value": 7.12, "unit": "pH" },
    "temperature": { "value": 25.4, "unit": "C" }
  }
}
```

`source` 当前可能出现：

- `realtime`
- `backfill`
- `realtime_no_sd`

### status

ESP32 上报在线状态，服务端会同步写入 `devices.status`。

### upload/status

ESP32 上报本地未上传数据状态。

示例：

```json
{
  "pending_count": 12,
  "failed_count": 2,
  "oldest_sample_time": "2026-05-14 08:30:00",
  "last_upload_at": "2026-05-14 10:20:00",
  "last_error": "PUB_TIMEOUT"
}
```

### commands/down

网页端通过服务端下发命令给设备。

当前已实现命令：

```text
UPLOAD_PENDING
```

当前实验第一版补传命令 payload：

```json
{
  "device_id": "ph_terminal_001",
  "command_name": "UPLOAD_PENDING",
  "payload": {
    "month": "2026-06",
    "force": true
  }
}
```

说明：

- `month`：指定补传月份
- `force: true`：强制从该月份 CSV 第一条开始重放

### commands/up

ESP32 回传命令执行状态。

运行中示例：

```json
{
  "command_id": 123,
  "status": "running",
  "result": "start uploading pending samples"
}
```

完成示例：

```json
{
  "command_id": 123,
  "status": "done",
  "result": "uploaded 12 samples, failed 0"
}
```

失败示例：

```json
{
  "command_id": 123,
  "status": "failed",
  "result": "uploaded 5 samples, failed 2: PUB_TIMEOUT"
}
```

## 补传机制说明

当前网页端和 ESP32 的补传链路约定如下：

1. 网页管理员选择月份并点击补传
2. 服务端创建 `UPLOAD_PENDING` 命令并经 MQTT 发往设备
3. payload 中携带：

```json
{
  "month": "2026-06",
  "force": true
}
```

4. ESP32 收到后强制重放该月份 CSV 数据
5. ESP32 将每条历史记录重新发布到 `telemetry`
6. 服务端按 `device_id + sample_time` 幂等更新
7. 设备同步上报 `upload/status`
8. 网页刷新未上传状态和命令结果

### 关于 force 模式

这一版网页端已经固定按 `force: true` 下发补传命令。

设计目的：

- 即使 ESP32 的 cursor 已经走到末尾，仍然允许对指定月份强制重放
- 服务端已有幂等逻辑，因此强制重放不会造成重复样本无限增长
- ESP32 固件端已约定：`force: true` 不会把 cursor 往回写，只做重放

## 命令超时处理

服务端包含运行中命令超时清理逻辑：

- [server.js](E:/ph-water-server/server.js)

当前策略：

- 每 60 秒做一次 sweep
- 运行超时阈值为 5 分钟

如果设备长时间没有回传命令完成状态，命令会被服务端标记为超时失败。

## 已知限制

实验第一版当前已知限制如下：

- Session 为内存态，不适合多实例部署
- MQTT 没有做设备级 ACL 验证说明
- 默认设备仍偏单设备场景
- `sample_values` 现在采用“先删后插”的幂等更新方式，简单直接，但不追求极致性能
- `upgrade-upload-status.sql` 适合从旧版本升级；如果历史库结构已手工改动，需要先核对索引名
- 项目内已有一次人工去重处理的备份表：

```text
samples_duplicate_backup_20260514
```

这个表用于保留历史重复样本备份，属于迁移过程产物

## 运行检查建议

每次改动后，建议至少执行：

```bash
node --check server.js
node --check mqtt-client.js
node --check routes/samples.js
node --check routes/commands.js
node --check routes/upload-status.js
node --check services/sample-store.js
node --check services/upload-status-store.js
node --check public/app.js
```

## 第一版结论

截至实验第一版，系统已经具备“可观测、可补传、可回执、可去重”的基本闭环：

- 数据能上来
- 历史数据能查
- 补传状态能看
- 网页能触发补传
- 设备能回执
- 服务端能幂等落库

这意味着第一版已经可以作为后续实验、联调和迭代的稳定起点。
