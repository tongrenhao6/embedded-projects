# 更新日志（CHANGELOG）

> 本项目**未使用 Git**，本文件是传感器节点固件逐次改动的流水记录，与 `docs/superpowers/plans/`（历史计划）及 `F:\0Web\docs\PROJECT_RECORD.md`（全系统事实档案）配合使用。
>
> **维护规则**（含 AI 助手/codex）：每完成一项影响协议、硬件接线、传感器逻辑、构建配置或实机验证结果的改动，**当天**在顶部按日期追加小节；只写已完成并验证的事实，计划中的事项不得写成完成。文件时间戳可作为佐证，但推断性结论需标注“（推断）”。
>
> 系统全貌：节点固件（本目录）→（LoRa 26 字节帧）→ 网关固件 `F:\0GateWay\myproject` →（MQTT）→ 网页服务 `F:\0Web`。

## 2026-08-29

- 收尾打磨（注释与头文件接口梳理，未改协议）：
  - `frame_codec.h` 补充中文注释与倾倒状态位宏说明。
  - `sensor_service.h` 暴露 `sensor_service_get_i2c_bus()`；`tilt_sensor_algorithm.h` 注释完善。
  - `main/main.c` 接线梳理（倾倒 + 双 SHT30 + LoRa 主循环）。
- 注意：`build/Nodeproject.bin` 为 08-25 产物，早于本次源码改动，**上板前需重新编译**。

## 2026-08-25（推断，据 build 产物）

- `build/Nodeproject.bin`（15:27）生成，为当前最新一次成功编译；对应的源码改动未单独成文。

## 2026-08-09

- `sensor_service.c` 调整（与 08-04 倾倒功能联调相关的传感器服务改动，细节未单独成文）。

## 2026-08-04

- **MPU6050 倾倒检测落地**：
  - 新增 `tilt_sensor` 组件：I2C 驱动（MPU6050 @0x68，兼容 WHO_AM_I 0x70 的 MPU-6500 克隆，±2 g 量程）+ 倾角算法（`atan2`，45° 触发 / 35° 恢复迟滞，连续 3 次确认）。
  - 附算法主机测试 `test_tilt_sensor.c`。
  - `frame_codec.h` 增加倾倒状态位（bit 2 倾倒有效 / bit 3 当前倾倒），与网关侧同步。
  - `docs/protocol/sensor_frame_v1.md` 更新；实施计划见 `docs/superpowers/plans/2026-08-04-mpu6050-tilt-detection.md`。
  - 计划中的现场验收项（水平 ≈0°、倾倒 >45° 告警、<35° 恢复、网关侧仅 `status_flags` 变化）**尚未回填勾选**。

## 2026-07-30

- 新增 `lora_transport` 组件：UART1（TX=GPIO4 / RX=GPIO5、9600 8N1）向 DX-LR22 透明传输模块发送已编码帧；含 230 字节载荷上限与主机测试。

## 2026-07-27

- 新增 `sensor_service`：I2C0（箱内 SHT30 @0x44，SDA GPIO6 / SCL GPIO7）+ I2C1（箱外 SHT30 @0x44，SDA GPIO15 / SCL GPIO16），单次采样命令 `0x2400`、Sensirion CRC8 校验、百分之一度/百分之一个百分点单位输出；附 `sht30_codec` 主机测试。

## 2026-07-24

- 建立 ESP-IDF v5.5.4 工程骨架（target `esp32s3`，16 MB flash + 八线 PSRAM）。
- `frame_codec` 首版（26 字节帧编码/解码 + CRC-16/CCITT-FALSE），与网关侧同源，附主机测试。
- `board_support`（WS2812 RGB @GPIO48，启动熄灭）、`docs/project_structure.md`。

## 备注（接班须知）

- `components/app_common/` 与 `components/lora_service/` 为空占位：节点号（=1）、采样周期（约 2 s）与引脚目前写死在 `main.c` 和各驱动内。
- `docs/hardware/`、`docs/test_records/` 为空；接线文档缺失，MPU6050 必须确认 3.3 V 供电。
- LoRa 模块频率/SF/功率均未在软件中配置，两端模块需预先用厂家工具配置一致。
- 主机测试需单独 gcc 编译（无 `idf.py` 集成，无 CI）。
- 无 Wi-Fi/MQTT；节点经 LoRa 单向发送，无 ACK/下行。
