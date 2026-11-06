/* sensor_service.h - shared sensor data + collection entry */
#ifndef SENSOR_SERVICE_H
#define SENSOR_SERVICE_H

#include <stdint.h>

typedef struct {
  float    temp;       /* degC */
  float    humi;       /* %RH */
  uint16_t co2_ppm;
  uint16_t light_lux;
  uint8_t  soil_grade; /* 0..100 湿度百分比 */
  uint8_t  alarm_flags;
  uint32_t ts;         /* RTC timestamp at collect */
  uint8_t  valid;
} sensor_data_t;

extern volatile sensor_data_t g_sensor;

void sensor_service_init(void);
void sensor_service_collect(void);

/* CO2(终端): USART2 定长 6 字节 0x2C 帧接收完成回调；校验并更新最新 ppm。 */
void sensor_service_co2_on_frame(const uint8_t *f);
void sensor_service_co2_rx_byte(uint8_t b);      /* 终端: USART2 逐字节喂入 CO2 状态机 */

#endif
