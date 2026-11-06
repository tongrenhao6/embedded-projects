/* bsp_dht22.c - ported from 12_DHT22; DWT counter gives us timing without
 * touching SysTick (which is owned by FreeRTOS). IRQ masked around sampling. */
#include "bsp_dht22.h"
#include "main.h"

static void dwt_enable(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0U;
  DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void dwt_delay_us(uint32_t us)
{
  uint32_t clk = SystemCoreClock / 1000000U;
  uint32_t start = DWT->CYCCNT;
  uint32_t target = clk * us;
  while ((uint32_t)(DWT->CYCCNT - start) < target) { }
}

void bsp_dht22_init(void)
{
  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
  dwt_enable();
}

static int dht22_start(void)
{
  uint32_t t = 0U;

  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_RESET);
  HAL_Delay(18U);
  HAL_GPIO_WritePin(DHT22_GPIO_Port, DHT22_Pin, GPIO_PIN_SET);
  dwt_delay_us(20U);

  t = 0U;
  while (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == GPIO_PIN_SET)
  {
    dwt_delay_us(2U);
    if (++t > 100U) return -1;
  }
  t = 0U;
  while (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == GPIO_PIN_RESET)
  {
    dwt_delay_us(2U);
    if (++t > 50U) return -2;
  }
  t = 0U;
  while (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == GPIO_PIN_SET)
  {
    dwt_delay_us(2U);
    if (++t > 50U) return -3;
  }
  return 0;
}

static uint8_t dht22_byte(void)
{
  uint32_t t = 0U;
  uint8_t d = 0U;
  uint8_t idx;

  for (idx = 0U; idx < 8U; idx++)
  {
    t = 0U;
    while (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == GPIO_PIN_RESET)
    {
      dwt_delay_us(2U);
      if (++t > 30U) return 0xFFU;
    }
    dwt_delay_us(40U);
    if (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == GPIO_PIN_SET)
    {
      d |= (uint8_t)(1U << (7U - idx));
      t = 0U;
      while (HAL_GPIO_ReadPin(DHT22_GPIO_Port, DHT22_Pin) == GPIO_PIN_SET)
      {
        dwt_delay_us(2U);
        if (++t > 15U) return 0xFFU;
      }
    }
  }
  return d;
}

int bsp_dht22_read(float *humi, float *temp)
{
  uint8_t i;
  uint8_t buff[5];
  int ret;

  ret = dht22_start();
  if (ret != 0) return ret;

  __disable_irq();
  for (i = 0U; i < 5U; i++)
  {
    buff[i] = dht22_byte();
    if (buff[i] == 0xFFU)
    {
      __enable_irq();
      return -2;
    }
  }
  __enable_irq();

  if ((uint8_t)(buff[0] + buff[1] + buff[2] + buff[3]) != buff[4])
    return -3;

  *humi = (float)((buff[0] << 8) | buff[1]) / 10.0f;
  if (buff[2] & 0x80U)
    *temp = (float)(((buff[2] & 0x7FU) << 8) | buff[3]) / (-10.0f);
  else
    *temp = (float)((buff[2] << 8) | buff[3]) / 10.0f;

  return 0;
}
