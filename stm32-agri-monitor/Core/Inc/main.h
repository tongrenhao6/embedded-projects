/* main.h - Header for the integrated gateway project (CubeMX style) */
#ifndef __MAIN_H
#define __MAIN_H

/* Exported functions --------------------------------------------------------*/
void Error_Handler(void);

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"
#include "app_config.h"

/* Private defines -----------------------------------------------------------*/
/* LoRa LLCC68 控制引脚（与 16_Lora 工程一致，供 llcc68_stm32_port.h 使用） */
#define Lora_NSS_GPIO_Port     GPIOA
#define Lora_NSS_Pin           GPIO_PIN_4
#define Lora_Busy_GPIO_Port    GPIOB
#define Lora_Busy_Pin          GPIO_PIN_0
#define Lora_Reset_GPIO_Port   GPIOB
#define Lora_Reset_Pin         GPIO_PIN_1

//#if (ROLE_GATEWAY == 1U)
//#define Lora_DIO1_GPIO_Port   GPIOA
//#define Lora_DIO1_Pin         GPIO_PIN_11
//#else
#define Lora_DIO1_GPIO_Port   GPIOB
#define Lora_DIO1_Pin         GPIO_PIN_10
//#endif



/* DHT22 数据引脚（终端板：PC15） */
#define DHT22_GPIO_Port        GPIOC
#define DHT22_Pin              GPIO_PIN_15

/* 蜂鸣器（终端板：有源，PB9，高电平响） */
#define BEEP_GPIO_Port         GPIOB
#define BEEP_Pin               GPIO_PIN_9

/* 运行指示灯（预留，低电平点亮） */
#define LED1_GPIO_Port         GPIOA
#define LED1_Pin               GPIO_PIN_8
#define LED2_GPIO_Port         GPIOB
#define LED2_Pin               GPIO_PIN_15



#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
