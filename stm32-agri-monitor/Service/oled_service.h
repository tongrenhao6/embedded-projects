/* oled_service.h - SSD1306 UI: node screen / gateway screen */
#ifndef OLED_SERVICE_H
#define OLED_SERVICE_H

#include <stdint.h>
#include "sensor_service.h"

typedef struct {
  uint8_t  id;          /* node addr */ 
  uint8_t  valid;
  uint8_t  alarm;
  uint16_t temp_x10;
  uint16_t humi_x10;
  uint16_t co2_ppm;
  uint16_t light_lux;
  uint8_t  soil_grade;
} gw_slot_t;

void oled_service_init(void);
void oled_service_node_screen(const sensor_data_t *s, uint8_t lora_ok, uint32_t ts);
void oled_service_gw_screen(const gw_slot_t *slots, uint8_t count,
                            uint8_t lora_ok, uint8_t wifi_ok, uint32_t ts);

#endif
