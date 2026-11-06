/* sensor_service.c - collect DHT22/ADC/CO2 into shared g_sensor */
#include "sensor_service.h"
#include "bsp_dht22.h"
#include "bsp_adc.h"
#include "bsp_uart.h"
#include "bsp_rtc.h"
#include "log_service.h"
#include "main.h"

volatile sensor_data_t g_sensor;

static uint16_t g_co2_latest;
static uint8_t  g_co2_valid;

void sensor_service_init(void)
{
  g_sensor.valid = 0U;
  g_sensor.co2_ppm = 0U;
  g_sensor.light_lux = 0U;
  g_sensor.soil_grade = 1U;
  g_sensor.temp = 0.0f;
  g_sensor.humi = 0.0f;
  g_sensor.alarm_flags = 0U;
  g_co2_latest = 0U;
  g_co2_valid = 0U;
}

void sensor_service_co2_on_frame(const uint8_t *f)
{
  uint8_t cs = 0U, k;

  if (f[0] != 0x2CU) return;                 /* 帧头 */
  if (f[3] != 0x03U || f[4] != 0xFFU) return; /* 量程 0x03FF */
  for (k = 0U; k < 5U; k++) cs += f[k];
  if (cs != f[5]) return;                    /* 校验和 */

  g_co2_latest = (uint16_t)((f[1] << 8U) | f[2]);
  g_co2_valid  = 1U;
}

//状态机
/* ===== 新增：CO2 逐字节状态机（找头→收6字节→校验） ===== */
static uint8_t  co2_frame[6];
static uint8_t  co2_idx;

void sensor_service_co2_rx_byte(uint8_t b)
{
  if (co2_idx == 0U)
  {
    if (b == 0x2CU) { co2_frame[0] = b; co2_idx = 1U; }
    return;                                    /* 等 0x2C 头 */
  }
  co2_frame[co2_idx++] = b;
  if (co2_idx >= 6U)
  {
    co2_idx = 0U;
    sensor_service_co2_on_frame(co2_frame);    /* 帧头/量程/校验不通过时也自动清零重找 */
  }
}

void sensor_service_collect(void)
{
  float t = 0.0f, h = 0.0f;
  int dret;

  dret = bsp_dht22_read(&h, &t);
  if (dret == 0)
  {
    g_sensor.temp = t;
    g_sensor.humi = h;
  }

  g_sensor.light_lux   = bsp_adc_get_lux();
  g_sensor.soil_grade  = bsp_adc_get_soil_grade();
  if (g_co2_valid != 0U) g_sensor.co2_ppm = g_co2_latest;

  g_sensor.ts = bsp_rtc_timestamp();
  g_sensor.valid = 1U;

  if (dret != 0) log_msg("sensor", "DHT22 read err=%d", dret);
  log_msg("sensor", "t=%.1f h=%.1f co2=%u lux=%u soil=%u",
          (double)g_sensor.temp, (double)g_sensor.humi,
          (unsigned)g_sensor.co2_ppm, (unsigned)g_sensor.light_lux,
          (unsigned)g_sensor.soil_grade);
}
