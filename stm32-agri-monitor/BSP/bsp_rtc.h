/* bsp_rtc.h - RTC free running 1s counter exposed as timestamp */
#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>

void     bsp_rtc_init(void);
uint32_t bsp_rtc_timestamp(void);

#endif
