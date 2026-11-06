/* ota.c - placeholder implementation, OTA feature reserved for later stage */
#include "ota.h"

void ota_init(void)
{
  /* 本阶段仅预留接口，不初始化任何 OTA 分区/过程 */
}

int16_t ota_check(void)
{
  return OTA_RESULT_NO_UPDATE;
}

int16_t ota_apply(void)
{
  return OTA_RESULT_UNSUPPORTED;
}
