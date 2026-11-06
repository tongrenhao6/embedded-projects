/* storage_service.c */
#include "storage_service.h"
#include "param.h"
#include "bsp_flash.h"
#include "log_service.h"

#include "protocol.h"

void storage_load(void)
{
  uint8_t *p = (uint8_t *)(uint32_t)PARAM_FLASH_ADDR;

  param_init_default();

  /* check magic first: unprogrammed flash is 0xFF */
  if (p[0] == 0xFFU)   /* never written */
  {
    storage_save();
    log_msg("storage", "defaults written");
    return;
  }

  /* read stored block */
  {
    uint32_t i;
    for (i = 0U; i < sizeof(g_param); i++)
      ((uint8_t *)&g_param)[i] = p[i];
  }

  if (param_is_valid() != 0U)
  {
    log_msg("storage", "params loaded from flash");
  }
  else
  {
    uint16_t calc = protocol_crc16((const uint8_t *)&g_param,
                                   (uint16_t)(sizeof(g_param) - 2U));
    log_msg("storage", "crc mismatch magic=%08X size=%u crc_saved=%04X crc_calc=%04X",
            (unsigned)g_param.magic, (unsigned)g_param.size,
            (unsigned)g_param.crc, (unsigned)calc);
    param_init_default();
    storage_save();
    log_msg("storage", "crc mismatch, defaults re-written");
  }
}

void storage_save(void)
{
  param_update_crc();
  (void)bsp_flash_erase_page(PARAM_FLASH_ADDR);
  (void)bsp_flash_write(PARAM_FLASH_ADDR, (const uint8_t *)&g_param,
                        (uint32_t)sizeof(g_param));
  log_msg("storage", "params saved");
}
