# 蜂箱物联网智能监测系统（毕业设计）

基于 ESP32-S3 与 LoRa 的「传感节点 → 无线网关 → 云端监控」低功耗蜂箱监测系统，实现温湿度实时感知与倾倒告警。端-边-云全链路独立完成。

## 系统架构

```
┌─────────────────┐    LoRa     ┌─────────────────┐    MQTT/TLS    ┌─────────────────┐
│  蜂箱传感节点     │ ──────────▶ │   LoRa 网关      │ ─────────────▶ │  云端 FastAPI    │
│  (ESP32-S3)      │  26 字节帧   │  (ESP32-S3)     │   QoS1 / LWT   │  + MySQL        │
│                  │             │                 │                │                 │
│  双 SHT30        │             │  UART 帧同步     │                │  遥测入库校验    │
│  MPU6050 倾角    │             │  Wi-Fi 重连     │                │  分钟级聚合     │
│  电池供电        │             │  MQTT 上云      │                │  Web 监控页     │
└─────────────────┘             └─────────────────┘                └─────────────────┘
```

## 已实现功能

**传感节点（node/）**
- 双 SHT30（I2C + CRC8）采集箱内/箱外温湿度
- MPU6050 倾倒检测：`atan2` 倾角解算，45° 触发告警 / 35° 迟滞恢复，连续确认防误报
- 自定义 26 字节大端帧协议（CRC-16 校验），经 LoRa 上报网关
- 低功耗设计，电池供电

**LoRa 网关（gateway/）**
- UART 帧同步、失步重同步、错误统计
- Wi-Fi 自动重连 + MQTT QoS1 / LWT（Last Will）
- RAM 队列溢出策略，断网不丢关键帧
- 上行转发到云端 MQTT broker

**云端服务（server/）**
- FastAPI 订阅 MQTT 遥测，严格校验后写 MySQL
- 分钟级聚合 + 内存实时缓存
- REST API + 原生 Web 监控页（实时数据、历史曲线、设备状态）
- 打包 Windows EXE 一键部署（PyInstaller）

## 目录结构

```
beehive-monitor-system/
├── node/                  # ESP32-S3 传感节点（ESP-IDF）
│   ├── main/main.c
│   └── components/
│       ├── sensor_service/    # SHT30 + MPU6050 采集
│       ├── tilt_sensor/       # 倾角算法（atan2 + 迟滞）
│       ├── frame_codec/       # 26 字节协议编解码
│       ├── lora_service/     # LoRa 收发
│       └── board_support/
├── gateway/                # ESP32-S3 LoRa 网关（ESP-IDF）
│   ├── main/main.c
│   └── components/
│       ├── serial_gateway/    # UART 帧同步
│       ├── lora_service/      # LoRa 接收
│       ├── mqtt_service/      # MQTT QoS1/LWT
│       ├── wifi_service/      # Wi-Fi 重连
│       └── mqtt_payload/
└── server/                 # 云端 FastAPI + MySQL
    ├── beehive_gateway/
    │   ├── mqtt_ingest.py    # MQTT 订阅与入库校验
    │   ├── storage.py        # MySQL 持久化
    │   ├── telemetry_state.py
    │   ├── api.py            # FastAPI 路由
    │   ├── mqtt_protocol.py
    │   └── web/              # 原生 JS 监控页
    ├── db/                   # SQL 建表脚本
    ├── tests/
    └── pyproject.toml
```

## 关键技术点

- **自定义应用层协议**：26 字节大端帧（设备 ID + 温湿度 + 倾角 + CRC-16），节点/网关/云端三端统一
- **倾倒防抖**：45° 触发 → 必须回到 35° 以下才恢复 → 连续 N 次确认才告警，避免蜂箱晃动误报
- **MQTT 可靠性**：QoS1 + LWT + 网关 RAM 队列溢出策略，Wi-Fi 抖动不丢帧
- **数据校验**：云端入库前严格校验字段范围与 CRC，防止脏数据
- **可部署性**：Python 端打包成 Windows EXE，双击即用，适合非技术人员使用

## 技术栈

ESP32-S3 / ESP-IDF / FreeRTOS / LoRa / MQTT / SHT30 / MPU6050 / FastAPI / MySQL / 原生 JS / PyInstaller
