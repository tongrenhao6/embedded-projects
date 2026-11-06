/* oled_service.c - uses BSP/OLED SSD1306 128x64 (I2C1) library */
#include "oled_service.h"
#include "oled.h"
#include "log_service.h"
#include <stdio.h>

void oled_service_init(void)
{
  OLED_Init();
  OLED_Clear();
  OLED_ShowString(0, 0, (uint8_t*)"Boot", 8);
  OLED_Clear();
}

void oled_service_node_screen(const sensor_data_t *s, uint8_t lora_ok, uint32_t ts)
{
  char line[17];

  snprintf(line, sizeof(line), "T:%5.1f  H:%5.1f", (double)s->temp, (double)s->humi);
  OLED_ShowString(0, 0, (uint8_t*)line, 8);

  snprintf(line, sizeof(line), "CO2:%5u  L:%3u", (unsigned)s->co2_ppm, (unsigned)s->light_lux);
  OLED_ShowString(0, 1, (uint8_t*)line, 8);

  snprintf(line, sizeof(line), "Soil:%d  Alarm:%d", (int)s->soil_grade, (int)s->alarm_flags);
  OLED_ShowString(0, 2, (uint8_t*)line, 8);

  snprintf(line, sizeof(line), "Lora:%s  ts:%lu", lora_ok ? "OK" : "--",
           (unsigned long)ts);
  OLED_ShowString(0, 3, (uint8_t*)line, 8);
}

void oled_service_gw_screen(const gw_slot_t *slots, uint8_t count,
                            uint8_t lora_ok, uint8_t wifi_ok, uint32_t ts)
{
  char line[24];
  uint8_t i;
  uint8_t row = 0U;

  snprintf(line, sizeof(line), "Lora:%s Wifi:%s", lora_ok ? "OK" : "--",
           wifi_ok ? "OK" : "--");
  OLED_ShowString(0, row++, (uint8_t*)line, 8);

  snprintf(line, sizeof(line), "ts:%lu", (unsigned long)ts);
  OLED_ShowString(0, row++, (uint8_t*)line, 8);

  for (i = 0U; (i < count) && (row < 6U); i++)
  {
    const gw_slot_t *p = &slots[i];
    if (p->valid != 0U)
    {
      snprintf(line, sizeof(line), "#%u T:%u.%u H:%u.%u", p->id,
               p->temp_x10 / 10U, p->temp_x10 % 10U,
               p->humi_x10 / 10U, p->humi_x10 % 10U);
      OLED_ShowString(0, row++, (uint8_t*)line, 8);
      snprintf(line, sizeof(line), " C:%u L:%u S:%d    ", p->co2_ppm, p->light_lux,
               p->soil_grade);
      OLED_ShowString(0, row++, (uint8_t*)line, 8);
	  snprintf(line,sizeof(line)," A:%d",p->alarm);
	  OLED_ShowString(0,row++,(uint8_t*)line,8);
    }
    else
    {
      snprintf(line, sizeof(line), "#%u --", p->id);
      OLED_ShowString(0, row++, (uint8_t*)line, 8);
    }
  }
}
