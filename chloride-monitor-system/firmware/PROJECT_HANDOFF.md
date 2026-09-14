# ESP32-S3 pH 水质监测项目交接文档

> 新对话接手本项目时，请先阅读本文档，再阅读具体源码。
> 本文档用于快速了解项目功能、硬件连接、代码结构、当前协议和最近改动。
> 后续每次修改固件功能、MQTT 协议、硬件引脚、存储格式或验证流程时，都应同步更新本文档。

## 1. 项目目标

本项目基于 ESP32-S3 开发板，实现 pH 水质监测终端。

核心功能：

- 通过 RS485 转 TTL 模块读取 pH 带温补传感器数据。
- 采集 pH、mV、温度。
- 使用 Wi-Fi 连接网络。
- 使用 MQTT 上传数据到 Node 后端和网页端。
- 使用 SD 卡本地保存采样 CSV。
- 网络异常时，后续可从 SD 卡补传未上传数据。
- SD 卡异常但 MQTT 在线时，本次采样会尝试直接 MQTT 上传，避免立即丢失。
- LVGL 屏幕提供实时测量页和数据管理页。

当前项目目录不是 Git 仓库。

## 2. 开发环境与目标芯片

- ESP-IDF：5.1.2
- Target：ESP32-S3
- Flash：16MB
- PSRAM：启用，Octal 80MHz
- 分区表：`partitions-16MiB.csv`
- 主工程名：`bootloader`

关键配置文件：

- `sdkconfig`
- `dependencies.lock`
- `partitions-16MiB.csv`
- `main/CMakeLists.txt`
- `main/idf_component.yml`

## 3. 当前硬件连接

### RS485 pH 传感器

pH 传感器使用 12V 电源适配器独立供电，传感器 RS485 A/B 接入 RS485 转 TTL 模块，再接 ESP32-S3。

当前固件配置：

| 功能 | ESP32-S3 引脚 |
|---|---:|
| RS485 UART TX | GPIO16 |
| RS485 UART RX | GPIO17 |
| UART | UART1 |
| 波特率 | 9600 |
| 数据格式 | 8N1 |
| 默认 Modbus 地址 | 3 |

配置位置：

- `main/APP_DEMO/ph_config.h`
- `main/APP_DEMO/ph_sensor_rs485.c`

注意：

- GPIO16/GPIO17 和板载摄像头 D5/D6 复用。当前摄像头功能已禁用，但如果后续恢复摄像头，需要重新规划 RS485 引脚。

### SD 卡

SD 卡使用 SPI2。

| 功能 | ESP32-S3 引脚 |
|---|---:|
| SPI MOSI | GPIO11 |
| SPI CLK | GPIO12 |
| SPI MISO | GPIO13 |
| SD CS | GPIO2 |

驱动位置：

- `components/BSP/SDIO/spi_sdcard.c`
- `components/BSP/SDIO/spi_sdcard.h`

### 计划加有线网口时的推荐 W5500 接线

推荐使用 W5500 SPI 网口模块，不推荐优先使用 LAN8720/RMII 方案。

W5500 可共用 SD 卡 SPI 总线：

| W5500 | ESP32-S3 引脚 | 说明 |
|---|---:|---|
| VCC | 3.3V | 使用 3.3V 电平模块 |
| GND | GND | 共地 |
| SCK | GPIO12 | 共享 SPI CLK |
| MOSI | GPIO11 | 共享 SPI MOSI |
| MISO | GPIO13 | 共享 SPI MISO |
| CS | GPIO8 | W5500 独立片选 |
| RST | GPIO18 | 推荐接 |
| INT | GPIO15 | 推荐接 |

## 4. 启动流程

入口文件：

- `main/main.c`

启动顺序：

1. 初始化 NVS。
2. 初始化 Wi-Fi。
3. Wi-Fi 成功后初始化 SNTP 时间同步。
4. 初始化 I2C、SPI、LED、按键、XL9555、24CXX、AP3216C、QMA6100P、LCD、文件扩展功能。
5. 进入 LVGL 主界面。
6. 在 `lvgl_demo()` 中启动采样任务和 MQTT 客户端。

关键入口：

- `main/main.c`
- `main/APP/lvgl_demo.c`

## 5. 主要源码结构

### pH 业务模块

目录：

- `main/APP_DEMO`

关键文件：

| 文件 | 作用 |
|---|---|
| `ph_config.h` | Wi-Fi、MQTT、设备 ID、RS485 引脚配置 |
| `ph_sensor_rs485.c/.h` | Modbus RTU 读取 pH 传感器 |
| `ph_data_model.c/.h` | 采样数据结构和状态枚举 |
| `ph_storage_sd.c/.h` | SD 卡 CSV 存储、查询、删除、上传游标 |
| `ph_mqtt_client.c/.h` | MQTT 连接、telemetry、状态、补传命令 |
| `ph_sampler.c/.h` | 定时采样、立即采样、SD 保存、自动上传 |
| `ph_wifi.c/.h` | Wi-Fi STA 连接 |
| `ph_time.c/.h` | SNTP 时间同步 |
| `lv_measure_demo.c` | 实时测量页面 |
| `lv_data_demo.c` | 数据管理页面 |

### 板级外设模块

目录：

- `components/BSP`

包含 LCD、I2C、SPI、SD、按键、LED、RTC、传感器等板级驱动。

## 6. RS485 采集流程

文件：

- `main/APP_DEMO/ph_sensor_rs485.c`

流程：

1. 初始化 UART1。
2. 按 Modbus RTU 发送读取请求：

   ```text
   addr 03 00 00 00 06 CRC_L CRC_H
   ```

3. 期望响应长度 17 字节：

   ```text
   addr 03 0C [12字节数据] CRC_L CRC_H
   ```

4. CRC16 校验。
5. 将 12 字节数据解析为 3 个 float：
   - pH
   - mV
   - temperature

已修复问题：

- 之前实时页面偶发三项显示 `0.00`，原因是 float 字节序尝试顺序错误，错误字节序解析出的极小值被当成合法数据。
- 当前优先使用大端寄存器顺序解析，示例帧：

  ```text
  03 03 0c 40 e8 00 00 c1 68 00 00 41 c7 33 33 b8 93
  ```

  解析结果约为：

  ```text
  pH = 7.25
  mV = -14.5
  temperature = 24.9
  ```

## 7. 采样策略

文件：

- `main/APP_DEMO/ph_sampler.c`

自动采样时间：

```text
00:00
04:00
08:00
12:00
16:00
20:00
```

每个时间点前 10 分钟内只采一次。

用户也可在“数据管理”页面触发立即采样。

采样成功后的流程：

```text
读取传感器
-> 封装 ph_sample_t
-> 优先写 SD 卡 current CSV 和 backup CSV
-> 写 SD 成功后，从 SD 卡按上传游标自动上传当月待上传数据
-> 写 SD 失败但 MQTT 在线时，直接发布本次 telemetry，source=sd_fail_direct
```

当前限制：

- 如果传感器读取失败，当前不会写入失败采样记录。
- 如果 SD 卡失败且 MQTT 也离线，本次采样仍没有持久兜底队列。后续可加内部 Flash/NVS 缓存。

## 8. SD 卡存储格式

文件：

- `main/APP_DEMO/ph_storage_sd.c`

目录结构：

```text
0:/PHLOG
0:/PHLOG/data
0:/PHLOG/data/current
0:/PHLOG/data/backup
0:/PHLOG/index
0:/PHLOG/tmp
```

按月保存：

```text
0:/PHLOG/data/current/ph_YYYY_MM.csv
0:/PHLOG/data/backup/ph_YYYY_MM_bak.csv
```

时间无效时：

```text
ph_unknown.csv
ph_unknown_bak.csv
```

CSV 表头：

```csv
date,collect_time,PH,temperature
```

示例：

```csv
2026-05-14,16:00:03,7.82,25.4
```

上传游标：

```text
0:/PHLOG/index/upload.cursor
```

格式：

```text
file=ph_2026_05.csv
sequence=0
line=12
```

说明：

- 当前 CSV 只保存 date、collect_time、PH、temperature。
- 内部 `ph_sample_t` 还包含 mV、传感器地址、状态、计数等，但暂未写入 CSV。

## 9. MQTT 配置与协议

配置位置：

- `main/APP_DEMO/ph_config.h`

当前配置：

```c
#define PH_DEVICE_ID       "ph_terminal_001"
#define PH_MQTT_BASE_TOPIC "phwater/v1"
```

主题格式：

```text
phwater/v1/devices/{device_id}/{suffix}
```

当前设备实际主题：

```text
phwater/v1/devices/ph_terminal_001/telemetry
phwater/v1/devices/ph_terminal_001/status
phwater/v1/devices/ph_terminal_001/upload/status
phwater/v1/devices/ph_terminal_001/commands/down
phwater/v1/devices/ph_terminal_001/commands/up
```

### telemetry

发布 topic：

```text
phwater/v1/devices/ph_terminal_001/telemetry
```

payload 示例：

```json
{
  "device_id": "ph_terminal_001",
  "sample_time": "2026-05-14 17:30:00",
  "date": "2026-05-14",
  "time": "17:30:00",
  "ph": 7.12,
  "temperature": 25.4,
  "source": "mqtt",
  "readings": {
    "ph": {
      "value": 7.12,
      "unit": "pH"
    },
    "temperature": {
      "value": 25.4,
      "unit": "C"
    }
  }
}
```

source 说明：

| source | 含义 |
|---|---|
| `mqtt` | 从 SD 卡 CSV 正常补传/上传 |
| `sd_fail_direct` | SD 卡保存失败后的实时直传 |
| `direct` | 备用直传来源 |

### status

MQTT 连接成功后发布：

```text
topic: phwater/v1/devices/ph_terminal_001/status
payload: online
```

注意：

- 之前在 MQTT 事件回调里直接统计 SD 卡状态导致 `mqtt_task` 栈溢出重启。
- 当前已改为事件回调只发布 online，并通知独立任务 `ph_mqtt_status` 异步发布 `upload/status`。

### upload/status

发布 topic：

```text
phwater/v1/devices/ph_terminal_001/upload/status
```

payload 示例：

```json
{
  "pending_count": 12,
  "failed_count": 1,
  "oldest_sample_time": "2026-05-14 09:10:00",
  "last_upload_at": "2026-05-14 17:35:00",
  "last_error": "PUB_TIMEOUT"
}
```

无待上传数据时：

```json
{
  "pending_count": 0,
  "failed_count": 0,
  "oldest_sample_time": null,
  "last_upload_at": "2026-05-14 17:35:00",
  "last_error": null
}
```

当前实现说明：

- `pending_count` 基于当前月份 CSV 和 `upload.cursor` 统计。
- 命令指定月份补传时，按指定月份统计。
- `failed_count` 不是持久失败记录，只表示当前运行期间最近补传失败计数；补传成功后清零。
- 项目暂未实现完整的失败记录队列。

### commands/down

订阅 topic：

```text
phwater/v1/devices/ph_terminal_001/commands/down
```

payload 示例：

```json
{
  "command_id": 23,
  "command_name": "UPLOAD_PENDING",
  "payload": {
    "month": "2026-05"
  }
}
```

支持命令：

| command_name | 功能 |
|---|---|
| `UPLOAD_PENDING` | 补传指定月份或当前月份的 SD 卡待上传数据 |

### commands/up

发布 topic：

```text
phwater/v1/devices/ph_terminal_001/commands/up
```

开始执行：

```json
{
  "command_id": 23,
  "status": "running",
  "result": "upload pending started"
}
```

执行成功：

```json
{
  "command_id": 23,
  "status": "done",
  "result": "uploaded 12 samples, failed 0"
}
```

执行失败：

```json
{
  "command_id": 23,
  "status": "failed",
  "result": "upload failed: PUB_TIMEOUT"
}
```

## 10. LVGL 页面

主界面：

- `main/APP_DEMO/app_ui.c`

实时测量页：

- `main/APP_DEMO/lv_measure_demo.c`

功能：

- 每秒读取一次 RS485 pH 传感器。
- 显示 pH、mV、温度、传感器地址/状态。

数据管理页：

- `main/APP_DEMO/lv_data_demo.c`

功能：

- 前一天/后一天选择日期。
- 立即采样。
- 查找指定日期记录数。
- 删除最后一条。
- 删除当天。
- 删除整月。
- 手动上传当前选中月份。
- 显示 SD、Wi-Fi、时间、采样状态、最近样本和待上传信息。

## 11. 最近关键改动记录

### 2026-05-14 附近

- 修复 RS485 float 字节序导致实时页面偶发三项全 `0.00` 的问题。
- 增加 SD 卡写入失败后的 MQTT 直传兜底：
  - `source=sd_fail_direct`
  - 状态：`DIRECT_UPLOADED` / `DIRECT_FAIL`
- MQTT telemetry 增加 `sample_time` 和 `readings` 字段，适配 Node 后端和网页。
- 增加 `upload/status` 上报。
- `UPLOAD_PENDING` 命令增加 `running` 状态回传。
- MQTT 连接成功发布 `online`。
- 修复 `mqtt_task` 栈溢出：
  - 不再在 MQTT 事件回调里执行 SD/FatFS 统计。
  - 新增 `ph_mqtt_status` 独立任务异步发布 `upload/status`。

## 12. 验证流程

### MQTT 全量监听

订阅：

```text
phwater/v1/devices/ph_terminal_001/#
```

### 上线状态

重启 ESP 后应收到：

```text
topic: phwater/v1/devices/ph_terminal_001/status
payload: online
```

随后应收到：

```text
topic: phwater/v1/devices/ph_terminal_001/upload/status
```

### telemetry

触发“立即采样”或等待自动采样，应收到：

```text
topic: phwater/v1/devices/ph_terminal_001/telemetry
```

关键字段：

```json
{
  "sample_time": "2026-05-14 18:20:00",
  "ph": 7.25,
  "temperature": 24.9,
  "source": "mqtt"
}
```

网页实时数据和趋势图应刷新。

### 待补传状态

制造未上传数据：

1. 保持 SD 卡可写。
2. 断开 MQTT broker 或断网。
3. 在数据管理页点几次“立即采样”。
4. 恢复 MQTT。
5. 重启 ESP 或等待重连。

应看到：

```json
{
  "pending_count": 1,
  "oldest_sample_time": "2026-05-14 18:20:00"
}
```

### 网页补传命令

网页点击“补传本月数据”后，应收到下行：

```json
{
  "command_id": 23,
  "command_name": "UPLOAD_PENDING",
  "payload": {
    "month": "2026-05"
  }
}
```

ESP 应回：

```json
{
  "command_id": 23,
  "status": "running",
  "result": "upload pending started"
}
```

结束后：

```json
{
  "command_id": 23,
  "status": "done",
  "result": "uploaded 12 samples, failed 0"
}
```

### SD 异常兜底

拔掉 SD 卡或让 SD 初始化失败，再触发立即采样。

如果 MQTT 在线，应收到：

```json
{
  "source": "sd_fail_direct"
}
```

数据管理页应显示“直传成功”。

### 重启/崩溃检查

不应再出现：

```text
***ERROR*** A stack overflow in task mqtt_task has been detected.
```

如果仍有重启，优先记录：

- panic 第一行
- task 名称
- backtrace
- 重启前最后 20 行日志

## 13. 后续建议

优先级较高：

1. 增加内部 Flash/NVS 临时缓存，解决“SD 卡失败 + MQTT 离线”时数据仍可能丢失的问题。
2. 给 `ph_sensor_read()` 增加互斥锁，避免实时页面和后台采样同时读 UART。
3. 将 Wi-Fi/MQTT 凭据从宏定义迁移到 NVS 配置或配网页面。
4. 如果加入 W5500，以太网优先，Wi-Fi 备用，MQTT 上层协议保持不变。
5. CSV 可扩展字段：mV、sensor_addr、sensor_status、upload_status。
6. 为 `upload/status.failed_count` 增加持久失败记录系统。

## 14. 文档维护规则

每次修改以下内容后必须更新本文档：

- 硬件引脚或外设连接。
- MQTT topic 或 payload 字段。
- SD 卡目录、CSV 格式或上传游标逻辑。
- 采样频率、采样策略或状态机。
- UI 页面功能或按钮行为。
- 重大 bug 修复和验证方法。
- 新增模块、任务、队列、互斥锁或定时器。

建议新对话开始时先说：

```text
请先阅读 PROJECT_HANDOFF.md，然后接着当前 ESP32-S3 pH 水质监测项目继续工作。
```
