# embedded-projects

嵌入式软件工程项目集 —— 葛玉彬（北部湾大学 · 物联网工程 · 2027 届）

## 项目列表

| 项目 | MCU / 平台 | 技术栈 | 简介 |
|---|---|---|---|
| [stm32-smart-home-panel](./stm32-smart-home-panel) | STM32F103C8T6 | CubeMX + HAL / Keil MDK / PWM / ADC / 编码器 / OLED | 智能家居控制面板：两路负载开关 + 光敏/编码器双模式调光 |
| [canteen-ordering-system](./canteen-ordering-system) | 6818 Linux 开发板 | LVGL 8.2 / framebuffer / evdev / TCP / epoll / 阿里云 | 校园点餐：触摸屏点餐 + epoll 服务端 16 路并发 |
| [chloride-monitor-system](./chloride-monitor-system) | ESP32-S3 | ESP-IDF / FreeRTOS / RS485 / SD 卡 / MQTT / Node.js / MySQL | 水质监测终端：采集→SD 缓存→MQTT 上云→Web 监控，断网补传 |

> 后续会陆续补入 ESP32 蜂箱毕设等项目。

## 技术栈速览

- **MCU**：STM32F1（Cortex-M3）、ESP32-S3（Xtensa LX7）
- **RTOS**：FreeRTOS
- **通信**：UART / I2C / SPI / ADC / PWM / LoRa / MQTT / TCP Socket(epoll)
- **GUI**：LVGL 8.x（framebuffer + evdev）
- **工具链**：STM32CubeMX、Keil MDK、ESP-IDF、VS Code、GCC、Git
- **调试**：J-Link / ST-Link、串口日志、示波器、逻辑分析仪

## 关于作者

2027 届物联网工程本科，求职方向为嵌入式软件工程师。
- 邮箱：1542756039@qq.com
- 电话：187-7732-5107
