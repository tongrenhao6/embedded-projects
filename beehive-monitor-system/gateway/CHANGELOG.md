# 更新日志（CHANGELOG）

> 本项目**未使用 Git**，本文件是网关固件逐次改动的流水记录，与 `docs/superpowers/plans/`（历史计划）及 `F:\0Web\docs\PROJECT_RECORD.md`（全系统事实档案）配合使用。
>
> **维护规则**（含 AI 助手/codex）：每完成一项影响协议、硬件接线、MQTT、构建配置或实机验证结果的改动，**当天**在顶部按日期追加小节；只写已完成并验证的事实，计划中的事项不得写成完成。文件时间戳可作为佐证，但推断性结论需标注“（推断）”。
>
> 系统全貌：节点固件 `F:\0Node\Nodeproject` →（LoRa 26 字节帧）→ 网关固件（本目录）→（MQTT）→ 网页服务 `F:\0Web`。

## 2026-09-02

- **MQTT 从 EMQX 公共 broker 切换到巴法云**：
  - `components/app_common/Kconfig` 新增 `BEEHIVE_MQTT_TOPIC_MODE`（`hierarchical` / `bemfa`）及巴法云平面主题、网关 ID 配置项。
  - `mqtt_payload` 新增 `mqtt_payload_copy_topic()`，支持平面遥测/状态主题；`mqtt_service` 按主题模式分支发布（QoS 1、状态 retained、LWT `offline` 语义不变）。
  - `sdkconfig.defaults` 同步新增巴法云默认项。
  - 本机 `sdkconfig` 由 EMQX 公共 broker（`mqtt://broker.emqx.io:1883`，旧值备份于 `sdkconfig.old`）切换为 `mqtt://mqttv2.bemfa.com:2023`；遥测主题 `beehivegw001node001telemetry`，状态主题 `beehivegw001status`，客户端 ID `beehive_gw_001`。Wi-Fi/MQTT 凭据仅存于本机 sdkconfig，不得提交或外传。
- 20:19 重新编译通过（`build/myproject.bin` 约 930 KB）。
- Web 侧同步改动见 `F:\0Web\CHANGELOG.md` 同日条目。

## 2026-08-11

- 接入 Wi-Fi 与 MQTT，完成网关全链路：
  - 新增 `wifi_service`（STA + 无限重连；SSID 为空时跳过启动，LoRa 仍可独立运行）。
  - 新增 `mqtt_service`（esp-mqtt 客户端、深度 8 的 RAM 发送队列、溢出丢弃计数、LWT）。
  - 新增 `mqtt_payload`（层级主题 `beehive/v1/...` 与 telemetry JSON 组装）及主机测试。
- `main/main.c` 完成 LoRa 收帧 → 串口 JSON → MQTT 入队的整体接线；每 5 s 心跳（暂不含 LoRa 收发统计）。
- 新增 `docs/protocol/mqtt_gateway_v1.md` 与 `docs/project_structure.md`。

## 2026-08-09

- LoRa 接收落地：新增 `lora_service`（UART1、9600 8N1、TX=GPIO4 / RX=GPIO5，接 DX-LR22 透明传输模块）与 `lora_stream_parser`（`0xB1` 帧同步、拆包恢复、CRC 失败重同步、错误统计），附主机测试。
- 新增 `serial_gateway`：有效帧以 `GWJSON {...}` 行输出到 USB 串口（调试通路），附主机测试。
- `board_support` 新增 WS2812 RGB 驱动（GPIO48，启动即熄灭）；`app_config.h` 更新引脚与保留 GPIO。

## 2026-08-04

- `frame_codec.h` 增加倾倒状态位（bit 2 倾倒有效 / bit 3 当前倾倒），与节点侧同步。
- 落地 `docs/protocol/sensor_frame_v1.md`（26 字节帧、大端、CRC-16/CCITT-FALSE）。

## 2026-07-23 / 07-24

- 建立 ESP-IDF v5.5.4 工程骨架（target `esp32s3`，16 MB flash + 八线 PSRAM）与组件目录。
- `board_support`（芯片/flash/PSRAM/堆/复位原因日志）、`app_common`（`app_config.h` 角色、心跳、引脚宏）。
- `frame_codec` 首版（26 字节帧编码/解码 + CRC），附 CMake。
- 写入总体计划 `docs/superpowers/plans/2026-07-24-beehive-monitoring-prototype.md`。**注意：该计划中 SPI SX1278、`pc_gateway` 等描述已被后来的 UART DX-LR22 方案和 `F:\0Web` 取代，仅作历史参考。**

## 备注（接班须知）

- 工作区内 `pc_gateway/`、`ml/`、`docs/test_records/` 为空占位：PC 端实际实现在 `F:\0Web`，GRU 温湿度预测尚未开始。
- 编译产物只保留最后一次：`build/myproject.bin` = 2026-09-02 20:19；更早的首次编译成功时间不可考。
- 串口烧录口注意：工作区 `.vscode` 配 COM11，`myproject/.vscode` 配 COM7，烧录前核对实际端口。
