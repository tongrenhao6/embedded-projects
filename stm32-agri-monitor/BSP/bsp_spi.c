/* bsp_spi.c */
#include "bsp_spi.h"
#include "spi.h"
#include "main.h"

void bsp_spi_init(void)
{
  MX_SPI1_Init();
}

HAL_StatusTypeDef bsp_spi_transmit(const uint8_t *tx, uint16_t len)
{
  return HAL_SPI_Transmit(&hspi1, (uint8_t *)tx, len, 100U);
}

HAL_StatusTypeDef bsp_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
  return HAL_SPI_TransmitReceive(&hspi1, (uint8_t *)tx, rx, len, 100U);
}
