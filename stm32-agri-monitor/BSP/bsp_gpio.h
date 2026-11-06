/* bsp_gpio.h - BEEP(PB9 active-high) + LEDs + LoRa DIO1 flag */
#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include "main.h"

extern volatile uint8_t lora_dio1_pending;

void bsp_gpio_init(void);
void beep_on(void);
void beep_off(void);
void beep_toggle(void);
void led1_on(void);
void led1_off(void);
void led1_toggle(void);
void led2_on(void);
void led2_off(void);
void led2_toggle(void);

#endif
