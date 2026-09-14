# ESP32-S3 pH 水质监测终端实验第一版说明文档

## 1. 项目概述

本项目是基于 ESP32-S3 开发板实现的 pH 水质监测终端实验第一版。

系统通过 RS485 接口读取带温补功能的 pH 传感器数据，经 RS485 转 TTL 模块接入 ESP32-S3，完成本地显示、SD 卡存储以及 MQTT 云端上传。

本版本定位为实验验证版，主要用于：

- 验证 pH 传感器读取是否稳定
- 验证 ESP32-S3 与 RS485 模块通信是否正常
- 验证 SD 卡本地存储是否可靠
- 验证 MQTT 实时上传是否正常
- 验证断网后本地留存、恢复网络后补传的整体流程

## 2. 硬件组成与连接关系

系统主要硬件包括：

- ESP32-S3 开发板
- RS485 pH 传感器（带温度补偿）
- RS485 转 TTL 电平模块
- 12V 电源适配器
- SD 卡
- LCD 显示屏

当前连接关系如下：

- pH 传感器由外部 `12V` 电源适配器供电
- 传感器 `RS485 A/B` 接入 `RS485 转 TTL` 模块
- RS485 转 TTL 模块再连接 ESP32-S3 的 UART1
- ESP32-S3 使用自带 WiFi 连接网络
- SD 卡用于本地保存采样记录

当前关键引脚配置：

- RS485 UART TX：`GPIO16`
- RS485 UART RX：`GPIO17`
- UART 端口：`UART1`
- 波特率：`9600`
- SD 卡 MOSI：`GPIO11`
- SD 卡 CLK：`GPIO12`
- SD 卡 MISO：`GPIO13`
- SD 卡 CS：`GPIO2`

相关文件：

- [ph_config.h](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_config.h)
- [ph_sensor_rs485.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_sensor_rs485.c)
- [spi_sdcard.c](E:/AESP32S3PP/36_bootloader/components/BSP/SDIO/spi_sdcard.c)

## 3. 软件整体架构

系统主要由以下模块组成：

- [main.c](E:/AESP32S3PP/36_bootloader/main/main.c)：系统启动入口
- [ph_wifi.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_wifi.c)：WiFi 连接与重连管理
- [ph_time.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_time.c)：SNTP 校时
- [ph_sensor_rs485.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_sensor_rs485.c)：RS485 传感器读取与 Modbus 数据解析
- [ph_sampler.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_sampler.c)：自动采样与立即采样
- [ph_storage_sd.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_storage_sd.c)：SD 卡 CSV 存储、游标管理、删除操作
- [ph_mqtt_client.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_mqtt_client.c)：MQTT 上报、状态上报、补传命令处理
- [lv_measure_demo.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/lv_measure_demo.c)：实时测量界面
- [lv_data_demo.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/lv_data_demo.c)：数据管理界面

当前启动流程如下：

1. 初始化 NVS 和底层外设
2. 后台启动 WiFi 初始化任务
3. 启动 LVGL 界面
4. 启动采样任务
5. WiFi 连上后执行 SNTP 校时
6. 校时后启动 MQTT 客户端

说明：

- 当前界面启动不会被 WiFi 阻塞
- 采样任务与 MQTT 启动已经解耦
- MQTT 连接成功后会主动上报设备在线状态和上传状态

## 4. 数据流说明

### 4.1 实时采样流程

当前实验第一版的实时采样流程为：

1. 读取传感器
2. 如果 MQTT 已连接，则优先直接上传当前这条实时数据
3. 再将这条数据写入 SD 卡
4. 如果实时上传成功且 SD 保存成功，则推进本地上传游标，避免这条数据继续被统计为未上传

这意味着：

- 网络在线时，优先保证网页端能收到当前采样数据
- SD 卡故障不再阻塞当前这条数据的实时上传

### 4.2 断网场景

当 WiFi 或 MQTT 不可用时：

- 当前采样数据仍然尝试写入 SD 卡
- 本地上传游标不会推进
- 数据会留在本地，等待后续手动补传

### 4.3 手动补传流程

当前补传为“按月份补传”：

- 数据来源：SD 卡中的月度 CSV
- 上传顺序：按 CSV 记录顺序上传
- telemetry 中 `source` 字段标记为 `backfill`

## 5. SD 卡存储结构

SD 卡目录结构如下：

```text
0:/PHLOG
0:/PHLOG/data/current
0:/PHLOG/data/backup
0:/PHLOG/index
0:/PHLOG/tmp
```

按月份保存的数据文件：

```text
0:/PHLOG/data/current/ph_YYYY_MM.csv
0:/PHLOG/data/backup/ph_YYYY_MM_bak.csv
```

如果采样时系统时间无效，则保存到：

```text
0:/PHLOG/data/current/ph_unknown.csv
0:/PHLOG/data/backup/ph_unknown_bak.csv
```

CSV 文件表头：

```csv
date,collect_time,PH,temperature
```

示例：

```csv
2026-06-13,08:00:01,7.21,25.3
```

上传游标文件位于：

```text
0:/PHLOG/index/*.cursor
```

例如：

```text
0:/PHLOG/index/ph_2026_06.cursor
```

游标的作用是记录当前月份文件已经被视为“已上传”的行数。

## 6. MQTT 配置与主题

当前设备标识配置：

- 设备 ID：`ph_terminal_001`
- MQTT 基础 topic：`phwater/v1`

设备使用的 topic：

- `phwater/v1/devices/ph_terminal_001/telemetry`
- `phwater/v1/devices/ph_terminal_001/status`
- `phwater/v1/devices/ph_terminal_001/upload/status`
- `phwater/v1/devices/ph_terminal_001/commands/down`
- `phwater/v1/devices/ph_terminal_001/commands/up`

### telemetry 示例

```json
{
  "device_id": "ph_terminal_001",
  "sample_time": "2026-06-13 08:00:01",
  "date": "2026-06-13",
  "time": "08:00:01",
  "ph": 7.21,
  "temperature": 25.3,
  "source": "realtime",
  "readings": {
    "ph": { "value": 7.21, "unit": "pH" },
    "temperature": { "value": 25.3, "unit": "C" }
  }
}
```

当前 `source` 含义：

- `realtime`：实时采样直接上传
- `backfill`：从 SD 卡补传
- `sd_fail_direct`：SD 写入失败，但本次数据直接通过 MQTT 上传成功

### upload/status 示例

```json
{
  "pending_count": 3,
  "failed_count": 0,
  "oldest_sample_time": "2026-06-13 08:00:01",
  "last_upload_at": "2026-06-13 10:20:00",
  "last_error": null
}
```

### commands/down 示例

```json
{
  "command_id": 54,
  "command_name": "UPLOAD_PENDING",
  "payload": {
    "month": "2026-06"
  }
}
```

### commands/up 示例

```json
{
  "command_id": 54,
  "status": "done",
  "result": "uploaded 5 samples for 2026-06"
}
```

## 7. 自动采样策略

当前自动采样时间点为：

- `00:00`
- `04:00`
- `08:00`
- `12:00`
- `16:00`
- `20:00`

系统会周期检查当前时间，在对应采样窗口内只采一次。

此外，数据管理页面还支持手动立即采样。

## 8. 当前界面功能

### 实时测量界面

相关文件：

- [lv_measure_demo.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/lv_measure_demo.c)

当前功能：

- 显示实时 pH
- 显示实时 mV
- 显示实时温度
- 显示传感器在线状态

### 数据管理界面

相关文件：

- [lv_data_demo.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/lv_data_demo.c)

当前功能：

- 立即采样
- 查询指定日期记录数
- 上传选中月份数据
- 删除选中日期最后一条记录
- 删除选中日期全部记录
- 删除选中月份全部记录
- 删除未知时间记录

## 9. 当前版本已知限制

以下问题是实验第一版必须明确说明的：

1. 首次开机未拿到有效时间时，会影响自动采样
   当前自动采样依赖有效时间，如果设备上电后一直没有完成校时，采样任务可能停留在 `WAIT_TIME`。

2. 未校时离线采样会生成 `ph_unknown.csv`
   这类数据不会进入当前的正常“按月份补传”流程。

3. 手动补传是按月份进行的
   如果网页下发的月份与真实待补传文件不一致，命令可能执行成功，但 `uploaded 0`。

4. pending 数据目前没有长期容量保护策略
   如果网络长时间不可用，本地 CSV 会持续累积，最终会受到 SD 卡容量限制。

5. 当前版本用于实验验证，不是最终量产版
   重点是验证链路是否打通，而不是完成全部复杂异常处理。

## 10. 实验第一版推荐测试流程

建议现场测试时按以下顺序进行：

1. 先在有 WiFi 的环境下上电
2. 等待设备拿到 IP 并完成一次时间同步
3. 确认本地时间显示正常
4. 进行一次在线采样，确认网页端能收到 telemetry
5. 保持不断电，断开 WiFi
6. 进行离线采样
7. 恢复 WiFi
8. 执行手动补传
9. 确认 pending 数量下降，网页端能看到 backfill 数据

不建议把“首次开机就完全没网、且未校时”作为本版本主要验证场景，因为这是当前版本的已知限制之一。

## 11. 调试时重点关注的文件

- [main.c](E:/AESP32S3PP/36_bootloader/main/main.c)
- [ph_sampler.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_sampler.c)
- [ph_mqtt_client.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_mqtt_client.c)
- [ph_storage_sd.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_storage_sd.c)
- [ph_time.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_time.c)
- [ph_wifi.c](E:/AESP32S3PP/36_bootloader/main/APP_DEMO/ph_wifi.c)

## 12. 版本定位

本版本应定义为：

- 实验第一版
- 可用于架构验证
- 可用于传感器、SD、MQTT、UI 联调验证
- 暂不作为最终离线健壮版发布

本版本的核心验证目标是：

- pH 传感器读取稳定
- SD 卡本地记录正常
- MQTT 实时上传正常
- 正常时间条件下 pending 统计有效
- 正常按月补传链路有效
