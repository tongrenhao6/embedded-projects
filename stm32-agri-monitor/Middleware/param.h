/* param.h - system parameters (RAM cache with defaults, persisted by storage) */
#ifndef PARAM_H
#define PARAM_H

#include <stdint.h>

//给 param_t 强制 1 字节对齐，消除 padding
#pragma pack(push,1)
typedef struct {
  uint32_t magic;           /* 魔数校验 */
  uint16_t size;            /* 结构大小 */
  uint8_t  node_id;         /* 本机节点地址 */
  uint8_t  node_count;      /* 网关管理的节点数量 */
  uint32_t collect_ms;      /* 采集周期 */
  uint32_t upload_ms;       /* 上传周期 */
  float    temp_high;       /* 温度上限 */
  float    temp_low;        /* 温度下限 */
  float    humi_high;       /* 湿度上限 */
  float    humi_low;        /* 湿度下限 */
  uint16_t co2_high;        /* CO2 上限(ppm) */
  uint16_t light_low;       /* 光照下限(lux) */
  uint8_t  soil_low;        /* 土壤湿度下限等级 */
  uint8_t  wifi_ssid[32];
  uint8_t  wifi_pswd[32];
  uint8_t  server_ip[16];   /* 网关 TCP 服务器 IP */
  uint16_t server_port;     /* 网关 TCP 服务器端口 */
  uint16_t lora_freq_mhz;   /* LoRa 频率(MHz) */
  uint8_t  lora_sf;         /* LoRa 扩频因子 */
  uint16_t crc;             /* 参数区 CRC16 */
} param_t;
#pragma pack(pop)

extern param_t g_param;

void    param_init_default(void);
void    param_update_crc(void);
uint8_t param_is_valid(void);

#endif
