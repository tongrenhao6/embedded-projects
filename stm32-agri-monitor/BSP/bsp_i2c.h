/* bsp_i2c.h - I2C1 wrapper (used by OLED) */
#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "stm32f1xx_hal.h"

void             bsp_i2c_init(void);
HAL_StatusTypeDef bsp_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len);
HAL_StatusTypeDef bsp_i2c_read(uint8_t dev_addr, uint16_t reg, uint8_t *data, uint16_t len);

#endif
