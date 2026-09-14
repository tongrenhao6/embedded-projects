# STM32 智能家居控制面板

基于 STM32F103C8T6 的课程设计项目，实现「两路开关负载 + 可调光主灯」的桌面控制面板，覆盖 STM32 常用外设：GPIO / PWM / ADC / 定时器编码器接口 / 软件 I2C / USART。

## 功能

- 两路负载（LED1 / LED2）独立开关控制
- 一路主灯 PWM 调光，亮度 0% – 100%
- 光敏电阻自动调光（环境越暗、主灯越亮）
- 旋转编码器手动调光，每次步进 5%
- OLED（128×64）实时显示当前选中通道、开关状态、亮度百分比、手动/自动模式
- USART1（115200 8N1）输出状态日志，便于联调

上电默认亮度 50%、自动模式；自动模式下旋转编码器立即切回手动。

## 硬件 / 引脚分配

| 引脚 | 功能 |
|---|---|
| PA0 (TIM2_CH1) | 主灯 PWM 输出 |
| PA1 / PA2 | LED1 / LED2 开关输出 |
| PA4 | 按键：选择当前控制对象 |
| PB13 | 按键：切换选中通道开关状态 |
| PB14 | 按键：切换手动 / 自动调光模式 |
| PB0 (ADC1_IN8) | 光敏电阻采样 |
| PA6 / PA7 (TIM3) | 旋转编码器 A / B 相 |
| PB8 / PB9 | OLED 软件模拟 I2C |
| PA9 / PA10 | USART1 调试串口 |

OLED 未使用硬件 I2C，而是在 PB8/PB9 上用 GPIO 模拟时序，方便移植到任意引脚。

## 工程结构

```
stm32-smart-home-panel/
├── Core/
│   ├── Inc/                # 头文件（main/gpio/adc/tim/usart/i2c/oled）
│   ├── Src/                # 用户代码
│   │   ├── main.c          # 主循环与业务逻辑
│   │   ├── oled.c          # OLED 软件 I2C 驱动 + 字模
│   │   ├── adc.c / tim.c / gpio.c / usart.c / i2c.c
│   └── Startup/            # 启动文件
├── Drivers/                # STM32 HAL + CMSIS（CubeMX 生成）
├── MDK-ARM/
│   ├── test.uvprojx        # Keil 工程
│   ├── test.uvoptx
│   └── startup_stm32f103xb.s
├── test.ioc                # STM32CubeMX 配置
└── STM32F103C8TX_FLASH.ld  # 链接脚本
```

## 编译与烧录

1. 用 STM32CubeMX 打开 `test.ioc`（可选，仅用于重新生成配置）
2. 用 Keil MDK 打开 `MDK-ARM/test.uvprojx`
3. 选择下载器（ST-Link / J-Link），编译后下载到芯片
4. 打开串口助手（115200 8N1）查看启动与按键日志

MDK 编译零错误零警告，已通过课程设计答辩。

## 关键实现点

- **PWM 调光**：TIM2 CH1，ARR 决定周期，CCR1 决定占空比，0–100% 线性映射
- **编码器接口**：TIM3 配置为 Encoder Mode，硬件自动计数，主循环读 CNT 差值
- **自动调光**：ADC 读光敏值，反映射到 20%–100% 亮度区间
- **按键消抖**：轮询 + `HAL_Delay(20)` + 等待释放（实现简单，后续可改 EXTI）
- **OLED**：GPIO 软件模拟 I2C 时序，4 行状态信息整屏刷新

## 已知不足

- 按键为阻塞式轮询，占用主循环时间
- OLED 整屏刷新，信息变多后需要局部刷新
- 部分引脚（PB1 / PA3 / PB15）配置后未使用，预留扩展
