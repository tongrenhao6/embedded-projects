# PC 网关数据采集设计

## 目标

在 `F:\0Web` 提供一个可前台运行的 Python 采集器：读取 ESP32 网关 USB 串口，只处理以 `GWJSON ` 开头的行，校验并写入 MySQL `beehive_gateway.sensor_readings`。

## 边界

- 普通 ESP-IDF 日志直接忽略。
- JSON 语法错误、协议字段错误和数据库异常记录日志，采集循环继续运行。
- 数据库唯一键 `(node_id, sequence_no)` 负责重复序号去重。
- 不在本阶段实现 Web API、网页或 Windows 服务。
- 密码只从环境变量读取，不写入源码、配置示例或日志。

## 结构

- `beehive_gateway/protocol.py`：GWJSON 行筛选、JSON 解码、字段校验。
- `beehive_gateway/storage.py`：MySQL 连接与参数化插入。
- `beehive_gateway/serial_ingest.py`：串口生命周期、逐行读取、日志和重连策略。
- `tests/`：纯主机单元测试，不要求真实串口或真实数据库。
- `db/bootstrap.sql`：数据库及表结构初始化脚本。

## 配置

环境变量：`BEEHIVE_SERIAL_PORT`、`BEEHIVE_DB_HOST`、`BEEHIVE_DB_PORT`、`BEEHIVE_DB_NAME`、`BEEHIVE_DB_USER`、`BEEHIVE_DB_PASSWORD`。

默认数据库名为 `beehive_gateway`，默认端口为 `3306`。串口端口必须显式指定，避免误读其他串口设备。
