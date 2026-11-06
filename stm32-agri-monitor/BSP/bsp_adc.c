/* bsp_adc.c - DMA circular, Ch0/Ch1 raw samples + GL5528 / soil lookup tables */
#include "bsp_adc.h"
#include "adc.h"
#include "main.h"

#include <math.h>

volatile uint16_t bsp_adc_dma_buf[2];


/*			均改为公式计算
typedef struct { uint32_t ohm; uint16_t lux; } photo_res_t;
typedef struct { uint32_t res; uint8_t  grade; } soil_res_t;

static const photo_res_t k_gl5528[] = {
  {100000U,   0U}, {70000U,   1U}, {50000U,   1U}, {40000U,   1U},
  {30000U,    2U}, {20000U,   4U}, {15000U,   5U}, {10000U,  10U},
  {7000U,    17U}, {5000U,   29U}, {4000U,   45U}, {3000U,   68U},
  {2000U,   124U}, {1000U,  350U},
};

static const soil_res_t k_soil[] = {
  {100000U, 1U}, {50000U, 2U}, {20000U, 3U}, {10000U, 4U},
};
*/

static uint32_t raw_to_res(uint16_t raw)
{
  float vol;
  uint32_t res;
  if (raw >= 4095U) return 100000UL;
  vol = (float)raw * 3.3f / 4096.0f;
  if (vol >= 3.3f) return 100000UL;
  res = (uint32_t)((10000.0f * vol) / (3.3f - vol));
  return res;
}

void bsp_adc_init(void)
{
  (void)HAL_ADC_Start_DMA(&hadc1, (uint32_t *)bsp_adc_dma_buf, 2U);
}

uint16_t bsp_adc_raw(int channel)
{
  return (channel == 0) ? bsp_adc_dma_buf[0] : bsp_adc_dma_buf[1];
}


//光照强度改为由公式计算
//lux = 350·(1000/R)^1.55
uint16_t bsp_adc_get_lux(void)
{
  uint32_t res = raw_to_res(bsp_adc_dma_buf[0]);
  double lux = 350.0 * pow(1000.0 / (double)res, 1.55);
  if (lux > 350.0) lux = 350.0;
  if (lux < 0.0)   lux = 0.0;
  return (uint16_t)(lux + 0.5);
}


//改为由公式计算
uint8_t bsp_adc_get_soil_grade(void)
{
  uint32_t res = raw_to_res(bsp_adc_dma_buf[1]);
  double    pct;

  if (res < 1U) res = 1U;                    /* 防 log10(0) 未定义 */

  pct = 100.0 * (5.0 - log10((double)res));   /* R=10k->100%; R=100k->0% */
  if (pct > 100.0) pct = 100.0;
  if (pct < 0.0)   pct = 0.0;

  return (uint8_t)(pct + 0.5);
}
