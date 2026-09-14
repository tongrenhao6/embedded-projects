# 水质监测终端 · ESP32-S3 + MQTT + Node.js/MySQL

基于 ESP32-S3 的水质（pH / 氯离子）在线监测终端，实现「传感器采集 → SD 卡本地存储 → MQTT 上云 → Web 监控」全链路。设备支持断网本地缓存、联网后补传，服务端按 `device_id + sample_time` 幂等写入，避免补传重复。

> **项目背景**：本项目面向混凝土氯离子在线监测需求，但专用氯离子传感器尚未到位，因此先以易于采购的 pH 传感器打通"采集→存储→上云→Web 监控"完整链路，传感器驱动层做了抽象（`ph_sensor_rs485.c`），后续换传感器只需替换驱动与标定算法。

## 系统架构

```
ESP32-S3 终端
  ├─ RS485 水质传感器 (pH / 氯离子)
  ├─ SD 卡本地环形存储（断网时缓存）
  ├─ WiFi + MQTT (QoS1) 上报实时与补传数据
  └─ LVGL 本地界面（实时数据 / 历史 / 设备状态）
        │
        │ MQTT
        ▼
Node.js + Express 服务端
  ├─ MQTT client 订阅遥测 topic
  ├─ MySQL 8 按 device_id + sample_time 幂等写入
  ├─ 登录鉴权 / 角色（用户 / 管理员）
  └─ REST API + 原生 HTML/JS 监控页
        │
        ▼
浏览器：实时数据、pH/温度趋势图、补传管理
```

## 已实现功能

**固件侧（ESP32-S3 / ESP-IDF）**
- RS485 传感器定时采集（`ph_sensor_rs485.c`）
- SD 卡本地文件缓存，断网不丢数据（`ph_storage_sd.c`）
- WiFi 自动重连 + MQTT QoS1 上云（`ph_mqtt_client.c`）
- 采样调度与数据模型（`ph_sampler.c` / `ph_data_model.c`）
- LVGL 本地数据展示界面

**服务端侧（Node.js / Express / MySQL）**
- MQTT 订阅 + 遥测入库（`mqtt-client.js`）
- 登录会话、用户/管理员角色
- 采样数据分页查询、pH/温度趋势图
- 管理员网页端下发"指定月份补传"命令，设备重放 CSV
- `device_id + sample_time` 幂等去重，补传不重复

## 目录结构

```
chloride-monitor-system/
├── firmware/                # ESP32-S3 ESP-IDF 工程
│   ├── main/
│   │   ├── main.c
│   │   └── APP_DEMO/
│   │       ├── ph_sensor_rs485.c   # RS485 传感器驱动
│   │       ├── ph_sampler.c        # 采样调度
│   │       ├── ph_storage_sd.c     # SD 卡本地缓存
│   │       ├── ph_mqtt_client.c    # MQTT 上云
│   │       ├── ph_wifi.c           # WiFi
│   │       ├── ph_data_model.c     # 数据模型
│   │       └── app_ui.c            # LVGL 界面整合
│   ├── CMakeLists.txt / sdkconfig
│   ├── image/                      # LVGL 图标
│   └── database/water_monitor.sql  # 建表 SQL
└── server/                  # Node.js 监控后端
    ├── server.js / db.js / mqtt-client.js
    ├── routes/  services/  middleware/
    ├── public/              # 原生 HTML/CSS/JS 监控页
    ├── sql/
    └── package.json
```

> 注：`components/`（BSP、LVGL、esp32-camera、MJPEG）与 `managed_components/` 为第三方/板厂代码，未上传，按 `main/idf_component.yml` 与 ESP-IDF 包管理器拉取。

## 编译与运行

### 固件

```bash
# 需要 ESP-IDF v5.x
cd firmware
idf.py set-target esp32s3
idf.py build
idf.py -p COMx flash monitor
```

### 服务端

```bash
cd server
npm install
cp .env.example .env   # 填 MySQL / MQTT 配置
mysql -u root -p < sql/water_monitor.sql
node server.js
```

## 关键技术点

- **断网补传**：SD 卡按月份存 CSV，联网后按服务端指令重放
- **幂等写入**：`device_id + sample_time` 唯一索引，重复上报不产生脏数据
- **MQTT QoS1**：至少一次送达，配合服务端幂等保证不丢不重
- **RS485 半双工**：MODBUS-RTU 风格读寄存器，超时重发
- **LVGL 资源有限**：图片转 C 数组，局部刷新

## 技术栈

ESP32-S3 / ESP-IDF / FreeRTOS / LVGL / RS485 / SD 卡 / WiFi / MQTT / Node.js / Express 5 / MySQL 8
