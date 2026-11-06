/* alarm_service.c */
#include "alarm_service.h"
#include "param.h"
#include "bsp_gpio.h"
#include "log_service.h"

static uint8_t beep_mute = 0U;       /* 默认允许响 */
static uint32_t beep_mute_tick = 0U; /* 置静音时刻（HAL_GetTick） */

void alarm_service_init(void)
{
  beep_mute = 0U;
  //beep_off();
	led1_off();
}

uint8_t alarm_service_check(const sensor_data_t *s)
{
  uint8_t flags = 0U;

  if (s->temp > g_param.temp_high) flags |= ALARM_FLAG_TEMP_HIGH;
  if (s->temp < g_param.temp_low)  flags |= ALARM_FLAG_TEMP_LOW;
  if (s->humi > g_param.humi_high) flags |= ALARM_FLAG_HUMI_HIGH;
  if (s->humi < g_param.humi_low)  flags |= ALARM_FLAG_HUMI_LOW;
  if (s->co2_ppm > g_param.co2_high) flags |= ALARM_FLAG_CO2;
  if (s->light_lux < g_param.light_low) flags |= ALARM_FLAG_LIGHT;
  if (s->soil_grade < g_param.soil_low)  flags |= ALARM_FLAG_SOIL;

  return flags;
}

void alarm_service_beep(uint8_t flags)
{
  if (beep_mute != 0U)
  {
    //beep_off();
	  led1_off();
    return;
  }
  if (flags != 0U)
    //beep_on();
	led1_on();
  else
    //beep_off();
	led1_off();
}

void alarm_service_mute(uint8_t mute)
{
  beep_mute = (mute != 0U) ? 1U : 0U;
  beep_mute_tick = HAL_GetTick();
  //beep_off();
  led1_off();
}

/* 静音超时自动恢复允许响（NodeAlarmTask 周期调用） */
void alarm_service_poll(void)
{
  if ((beep_mute != 0U) &&
      ((uint32_t)(HAL_GetTick() - beep_mute_tick) >= BEEP_MUTE_AUTO_RESET_MS))
  {
    beep_mute = 0U;
    log_msg("alarm", "mute auto reset, allow beep");
    //beep_off();
	  led1_off();
  }
}
