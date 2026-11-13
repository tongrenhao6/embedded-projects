# 硬件设计（立创 EDA）

本目录存放智慧农业监测系统两块板的硬件设计资料，对应固件见上级目录。

## 概览

- 设计工具：立创 EDA（嘉立创 EDA）
- 板型：网关板（Board1）+ 终端板（Board2），两层板
- 主控：STM32F103C8T6 核心板插接，底板围绕核心板完成外设与电源连线
- 状态：嘉立创打样、焊接联调通过，配合上级目录固件实机运行

## 网关板 Board1

- 供电：Type-C 输入、自恢复保险丝、AMS1117-3.3 LDO、滤波电容
- 串口：CH340K USB 转串口
- 无线：ESP-12E（ESP8266）WiFi、Ra-01SC LoRa（SPI）
- 显示：OLED（I2C）
- 外设：按键、LED、排针接口
- 资料：`schematic-gateway.pdf`（原理图）、`pcb-layout-gateway.png`（PCB 布局）、`BOM-gateway.csv`（物料清单）

## 终端板 Board2

- 电源：Type-C、TP4056 锂电充电管理、MT3540 DC-DC、电池接口、防反/续流二极管
- 驱动：S8050 / SI2302 / AO3401 三极管与 MOS 管驱动继电器（负载/灌溉控制）
- 采集与告警：光敏电阻、蜂鸣器、传感器接线端子
- 无线：Ra-01SC LoRa（SPI）、CH340K 串口
- 资料：`BOM-node.csv`（终端板物料清单；原理图与 PCB 截图以网关板为准）

## 设计边界（如实说明）

- MCU 最小系统采用立创 STM32F103C8T6 核心板插接，底板未从头绘制 MCU 最小系统。
- 两层 DIY 板，未涉及多层板叠层、高速/射频阻抗控制、大功率电源板量产设计。
- PCB 工具为立创 EDA，未使用 Altium Designer / KiCad。
