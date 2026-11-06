/* param.c */
#include "param.h"
#include "protocol.h"
#include "app_config.h"
#include <string.h>

param_t g_param;

void param_init_default(void)
{
  memset(&g_param, 0, sizeof(g_param));
  g_param.magic      = 0x593250U;             /* "32P" */
  g_param.size       = (uint16_t)sizeof(g_param);

  /* 角色相关默认 */
#if (ROLE_GATEWAY == 1U)
  g_param.node_id    = NODE_ID_GATEWAY;
  g_param.node_count = DEFAULT_NODE_COUNT;
#else
  g_param.node_id    = NODE_ID_BASE;          /* 终端默认节点号 2 */
  g_param.node_count = 1U;
#endif

  g_param.collect_ms = DEFAULT_COLLECT_MS;
  g_param.upload_ms  = DEFAULT_UPLOAD_MS;

  g_param.temp_high  = DEFAULT_TEMP_HIGH;
  g_param.temp_low   = DEFAULT_TEMP_LOW;
  g_param.humi_high  = DEFAULT_HUMI_HIGH;
  g_param.humi_low   = DEFAULT_HUMI_LOW;
  g_param.co2_high   = DEFAULT_CO2_HIGH;
  g_param.light_low  = DEFAULT_LIGHT_LOW;
  g_param.soil_low   = DEFAULT_SOIL_LOW;

  memcpy(g_param.wifi_ssid, DEFAULT_WIFI_SSID, sizeof(DEFAULT_WIFI_SSID));
  memcpy(g_param.wifi_pswd, DEFAULT_WIFI_PSWD, sizeof(DEFAULT_WIFI_PSWD));
  memcpy(g_param.server_ip, DEFAULT_SERVER_IP, sizeof(DEFAULT_SERVER_IP));
  g_param.server_port = DEFAULT_SERVER_PORT;

  g_param.lora_freq_mhz = 470U;
  g_param.lora_sf       = 5U;
  param_update_crc();
}

void param_update_crc(void)
{
  g_param.crc = protocol_crc16((const uint8_t *)&g_param,
                               (uint16_t)(sizeof(g_param) - sizeof(uint16_t)));
}

uint8_t param_is_valid(void)
{
  uint16_t c = protocol_crc16((const uint8_t *)&g_param,
                              (uint16_t)(sizeof(g_param) - sizeof(uint16_t)));
  return (g_param.magic == 0x593250U) && (c == g_param.crc) ? 1U : 0U;
}
