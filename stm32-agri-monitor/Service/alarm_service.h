/* alarm_service.h - threshold detection + active buzzer */
#ifndef ALARM_SERVICE_H
#define ALARM_SERVICE_H

#include <stdint.h>
#include "sensor_service.h"

#define ALARM_FLAG_TEMP_HIGH  0x01U
#define ALARM_FLAG_TEMP_LOW   0x02U
#define ALARM_FLAG_HUMI_HIGH  0x04U
#define ALARM_FLAG_HUMI_LOW   0x08U
#define ALARM_FLAG_CO2        0x10U
#define ALARM_FLAG_LIGHT      0x20U
#define ALARM_FLAG_SOIL       0x40U

void    alarm_service_init(void);
uint8_t alarm_service_check(const sensor_data_t *s);
void    alarm_service_beep(uint8_t flags);
void    alarm_service_mute(uint8_t mute);
void    alarm_service_poll(void);   /* 静音超时自动恢复（任务周期调用） */

#endif
