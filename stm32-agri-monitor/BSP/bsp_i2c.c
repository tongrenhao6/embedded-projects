/* bsp_i2c.c */
#include "bsp_i2c.h"
#include "i2c.h"
#include "main.h"

void bsp_i2c_init(void)
{
  MX_I2C1_Init();
}

HAL_StatusTypeDef bsp_i2c_write(uint8_t dev_addr, uint8_t reg, const uint8_t *data, uint16_t len)
{
  return HAL_I2C_Mem_Write(&hi2c1, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, (uint8_t *)data, len, 100U);
}

HAL_StatusTypeDef bsp_i2c_read(uint8_t dev_addr, uint16_t reg, uint8_t *data, uint16_t len)
{
  return HAL_I2C_Mem_Read(&hi2c1, dev_addr, reg, I2C_MEMADD_SIZE_8BIT, data, len, 100U);
}
