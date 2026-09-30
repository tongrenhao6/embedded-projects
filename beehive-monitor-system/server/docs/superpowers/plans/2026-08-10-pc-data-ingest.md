# PC 网关数据采集实现计划

目标：实现可测试的 GWJSON 串口采集和 MySQL 持久化。

## 任务 1：协议解析

文件：`beehive_gateway/protocol.py`、`tests/test_protocol.py`

- 先写测试：有效 JSON、`null` 字段、普通日志、非法 JSON、缺少字段和错误类型。
- 运行 `pytest tests/test_protocol.py -q`，确认缺少实现时失败。
- 实现前缀筛选、JSON 解码和固定字段校验，返回结构化记录或明确异常。
- 重新运行该测试并保持输出无失败。

## 任务 2：MySQL 存储

文件：`beehive_gateway/storage.py`、`tests/test_storage.py`

- 先写使用内存假连接的测试，验证参数化插入、`None` 传递和重复序号不重复写入。
- 实现连接工厂、插入 SQL 和提交/关闭生命周期。
- 运行存储单元测试。

## 任务 3：串口采集器

文件：`beehive_gateway/serial_ingest.py`、`tests/test_ingest.py`

- 先写测试验证普通日志忽略、有效行写入、非法行继续处理。
- 实现逐行读取、异常日志和 `KeyboardInterrupt` 正常退出。
- 增加 `python -m beehive_gateway.serial_ingest` 命令入口。

## 任务 4：配置和交付

文件：`pyproject.toml`、`.env.example`、`README.md`

- 声明 `pyserial` 与 `mysql-connector-python` 依赖。
- 写出串口号、数据库账号配置和运行命令。
- 运行完整 `pytest -q`，再做 Python 编译检查。
