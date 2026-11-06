/* bsp_gpio.c - beep/led helpers; DIO1 EXTI callback sets flag only */
#include "bsp_gpio.h"

volatile uint8_t lora_dio1_pending = 0U;

void bsp_gpio_init(void)
{
  /* pins already configured in MX_GPIO_Init */
  beep_off();
  led1_off();
  led2_off();
}

void beep_on(void)      { HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_SET); }
void beep_off(void)     { HAL_GPIO_WritePin(BEEP_GPIO_Port, BEEP_Pin, GPIO_PIN_RESET); }
void beep_toggle(void)  { HAL_GPIO_TogglePin(BEEP_GPIO_Port, BEEP_Pin); }

void led1_off(void)     { HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET); }
void led1_on(void)      { HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); }
void led1_toggle(void)  { HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin); }

void led2_off(void)     { HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET); }
void led2_on(void)      { HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET); }
void led2_toggle(void)  { HAL_GPIO_TogglePin(LED2_GPIO_Port, LED2_Pin); }

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == Lora_DIO1_Pin)
  {
    lora_dio1_pending = 1U;
  }
}
