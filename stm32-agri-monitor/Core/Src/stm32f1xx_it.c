/* stm32f1xx_it.c - Interrupt handlers (shortest possible handling) */
#include "main.h"
#include "stm32f1xx_it.h"
#include "FreeRTOS.h"
#include "task.h"

#include "usart.h"
#include "i2c.h"
#include "spi.h"
#include "adc.h"

extern void xPortSysTickHandler(void);

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
  while (1) { }
}

void MemManage_Handler(void)
{
  while (1) { }
}

void BusFault_Handler(void)
{
  while (1) { }
}

void UsageFault_Handler(void)
{
  while (1) { }
}

void DebugMon_Handler(void)
{
}

/*
(void)SysTick->CTRL; 清溢出标志（与 cmsis_os2 官方一致）。
调度器未启动前不调用 xPortSysTickHandler()，杜绝空指针 HardFault；启动后照常调度。
xTaskGetSchedulerState 已在 task.h（it.c 已 #include "FreeRTOS.h" #include "task.h"），可直接用。
*/
void SysTick_Handler(void)
{
  (void)SysTick->CTRL;
  HAL_IncTick();
  HAL_SYSTICK_IRQHandler();
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    xPortSysTickHandler();
  }
}

void DMA1_Channel1_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma_adc1);
}

void USART1_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart1);
}

void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}

void EXTI15_10_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(Lora_DIO1_Pin);
}

void I2C1_EV_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&hi2c1);
}

void I2C1_ER_IRQHandler(void)
{
  HAL_I2C_ER_IRQHandler(&hi2c1);
}

void SPI1_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi1);
}

void ADC1_2_IRQHandler(void)
{
  HAL_ADC_IRQHandler(&hadc1);
}

