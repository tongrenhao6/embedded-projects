/* bsp_spi.h - SPI1 wrapper (LoRa LLCC68) */
#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "stm32f1xx_hal.h"

void             bsp_spi_init(void);
HAL_StatusTypeDef bsp_spi_transmit(const uint8_t *tx, uint16_t len);
HAL_StatusTypeDef bsp_spi_transceive(const uint8_t *tx, uint8_t *rx, uint16_t len);

#endif
