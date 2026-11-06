/* bsp_rtc.c - timestamp via SysTick (hardware RTC disabled this stage) */
#include "bsp_rtc.h"
#include "main.h"

void bsp_rtc_init(void)
{
  /* 不启用硬件 RTC：时间戳由 SysTick 提供 */
}

uint32_t bsp_rtc_timestamp(void)
{
  return HAL_GetTick() / 1000U;   /* 系统运行秒数 */
}