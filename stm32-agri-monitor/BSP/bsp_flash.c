/* bsp_flash.c - halfword-programmed internal flash (F1: min unit 16bit) */
#include "bsp_flash.h"
#include "main.h"

uint8_t bsp_flash_read(uint32_t addr, uint8_t *data, uint32_t len)
{
  uint32_t i;
  for (i = 0U; i < len; i++)
    data[i] = *((uint8_t *)addr + i);
  return 1U;
}

uint8_t bsp_flash_write(uint32_t addr, const uint8_t *data, uint32_t len)
{
  uint32_t i;
  uint16_t hw;
  if ((addr & 1U) != 0U) return 0U;          /* halfword boundary required */

  HAL_FLASH_Unlock();
  for (i = 0U; i < (len / 2U); i++)
  {
    hw = (uint16_t)((uint16_t)data[2U * i] | ((uint16_t)data[2U * i + 1U] << 8U));
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + 2U * i, hw) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return 0U;
    }
  }
  /* odd tail byte */
  if ((len & 1U) != 0U)
  {
    hw = data[len - 1U];
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + len - 1U, hw) != HAL_OK)
    {
      HAL_FLASH_Lock();
      return 0U;
    }
  }
  HAL_FLASH_Lock();
  return 1U;
}

uint8_t bsp_flash_erase_page(uint32_t page_addr)
{
  FLASH_EraseInitTypeDef erase = {0};
  uint32_t page_err = 0U;

  erase.TypeErase   = FLASH_TYPEERASE_PAGES;
  erase.PageAddress = page_addr;
  erase.NbPages     = 1U;

  HAL_FLASH_Unlock();
  if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK)
  {
    HAL_FLASH_Lock();
    return 0U;
  }
  HAL_FLASH_Lock();
  return 1U;
}
