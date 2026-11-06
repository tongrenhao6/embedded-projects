/* main.c - Application entry (integrated project, single project dual firmware) */
#include "main.h"
#include "usart.h"
#include "i2c.h"
#include "spi.h"
#include "adc.h"
#include "dma.h"
#include "rtc.h"
#include "gpio.h"
#include "app_freertos.h"
#include "bsp_uart.h"
#include "bsp_adc.h"
#include "bsp_rtc.h"
#include "bsp_dht22.h"
#include "bsp_flash.h"
#include "bsp_gpio.h"
#include "lora_service.h"
#include "storage_service.h"
#include "oled_service.h"
#include "log_service.h"
#include <stdio.h>

struct __FILE { int handle; };
FILE __stdout;
FILE __stdin;

//放弃 HAL_UART_GetState 和 HAL 发送，直接用 SR/DR 寄存器发，TXE 标志天然字节级互斥，不怕并发，也不依赖 HAL 状态
int fputc(int ch, FILE *f)
{
  while ((USART1->SR & USART_SR_TXE) == 0U) { }  /* 等发送数据寄存器空 */
  USART1->DR = (uint8_t)ch;                        /* 写一个字节 */
  return ch;
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}


extern void node_app_init(void);
extern void gateway_app_init(void);

int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();
  MX_DMA_Init();
  //MX_RTC_Init();   /* RTC 已降级为 SysTick 软件时间戳 */
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();

  log_init();
  bsp_uart_init();
  bsp_adc_init();
  bsp_rtc_init();
  storage_load();

#if (ROLE_GATEWAY == 1U)
  gateway_app_init();		//网关
#else
  node_app_init();			//终端
#endif

  MX_FREERTOS_Init();
  osKernelStart();

  while (1) { }
}




void Error_Handler(void)
{
  __disable_irq();
  while (1) { }
}
