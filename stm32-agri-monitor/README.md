# STM32 农业环境监测终端与网关

本工程基于 STM32F103C8、STM32 HAL 和 FreeRTOS，提供两种固件角色：

- **终端**：采集 DHT22、CO2、光照和土壤湿度数据，按阈值进行本地告警，并通过 LLCC68 LoRa 回复网关轮询。
- **网关**：轮询终端，使用 ESP8266 AT 指令连接网络，通过 MQTT 向 OneNET 上报属性，并在 OLED 上显示状态。

通过 `Core/Inc/app_config.h` 中的 `ROLE_GATEWAY` 切换固件角色：`0` 为终端，`1` 为网关。通信帧格式及 CRC16 实现在 `Middleware/protocol.c`。

## 目录

| 目录 | 内容 |
| --- | --- |
| `App/` | 终端与网关任务 |
| `BSP/` | 板级外设、LLCC68 与 OLED 驱动 |
| `Core/` | 启动流程、外设初始化、FreeRTOS 任务配置 |
| `Middleware/` | 通信协议、参数和环形缓冲区 |
| `Service/` | 传感器、LoRa、Wi-Fi、显示、告警与存储服务 |
| `MDK-ARM/` | Keil MDK 工程与启动文件 |

## 构建与配置

1. 使用 Keil MDK 打开 `MDK-ARM/32project.uvprojx`，目标芯片为 STM32F103C8。
2. 在 `Core/Inc/app_config.h` 中设置 `ROLE_GATEWAY`，分别编译终端和网关固件。
3. 在网关上云前，将该文件中的 `YOUR_WIFI_*`、`YOUR_PRODUCT_ID`、`YOUR_DEVICE_NAME` 和 `YOUR_DEVICE_TOKEN` 等占位值替换为自己的配置。**不要把真实账号凭据提交到公开仓库。**
4. 按实际模块型号核对 USART2 波特率、传感器接口及 LoRa 参数，再烧录到对应板卡。

## 当前实现边界

- 默认配置为一个网关、一个终端。虽然预留多节点轮询字段，上云数据和远程控制当前只针对第一个终端。
- 本地告警当前使用 LED1；蜂鸣器接口尚未接入当前告警输出。
- 网关上报时间使用固定基准加运行时间，尚未接入网络校时。
- 远程参数配置和 OTA 仅预留接口；自动灌溉控制未实现。

源码中包含 HAL、CMSIS、FreeRTOS 和 LLCC68 等第三方组件，保留了对应的版权声明和许可证。当前目录仅提供源码与工程文件，未包含历史编译产物。硬件联调结果需以实际复测为准。
