/* bsp_flash.h - last-page parameter storage on internal flash */
#ifndef BSP_FLASH_H
#define BSP_FLASH_H

#include <stdint.h>

/* F103C8: 64KB flash, use last page (1KB) for parameters */
#define PARAM_FLASH_ADDR   0x0800FC00U
#define PARAM_FLASH_SIZE   1024U

uint8_t bsp_flash_read(uint32_t addr, uint8_t *data, uint32_t len);
uint8_t bsp_flash_write(uint32_t addr, const uint8_t *data, uint32_t len);
uint8_t bsp_flash_erase_page(uint32_t page_addr);

#endif
