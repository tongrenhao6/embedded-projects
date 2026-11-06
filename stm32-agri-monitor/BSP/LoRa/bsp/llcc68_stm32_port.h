#ifndef LLCC68_STM32_PORT_H
#define LLCC68_STM32_PORT_H

/*
 * STM32CubeMX 工程的板级映射。
 * 只需修改以下定义，使其与 CubeMX 生成的 GPIO 标签一致。
 */
#include "main.h"
#include "spi.h"

extern SPI_HandleTypeDef hspi1;
/* LLCC68 使用的 STM32 HAL SPI 句柄。 */
#define LLCC68_SPI_HANDLE              hspi1

/* CubeMX 生成的 LLCC68 控制和状态引脚。 */
#define LLCC68_NSS_GPIO_Port           Lora_NSS_GPIO_Port
#define LLCC68_NSS_Pin                 Lora_NSS_Pin
#define LLCC68_BUSY_GPIO_Port          Lora_Busy_GPIO_Port
#define LLCC68_BUSY_Pin                Lora_Busy_Pin
#define LLCC68_RESET_GPIO_Port         Lora_Reset_GPIO_Port
#define LLCC68_RESET_Pin               Lora_Reset_Pin
#define LLCC68_DIO1_GPIO_Port          Lora_DIO1_GPIO_Port
#define LLCC68_DIO1_Pin                Lora_DIO1_Pin

/* 正常情况下，BUSY 应在数毫秒内变为低电平。 */
#define LLCC68_BUSY_TIMEOUT_MS         100U
#define LLCC68_SPI_TIMEOUT_MS          100U

#endif
