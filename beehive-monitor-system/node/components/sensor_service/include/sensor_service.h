#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    int16_t temperature_centi_c;    // 温度值，单位为摄氏度的百分之一
    uint16_t humidity_centi_pct;     // 湿度值，单位为百分比的百分之一
} sensor_service_sht30_reading_t;

esp_err_t sensor_service_init(void);
esp_err_t sensor_service_read_inside_sht30(sensor_service_sht30_reading_t *reading);
esp_err_t sensor_service_read_outside_sht30(sensor_service_sht30_reading_t *reading);
i2c_master_bus_handle_t sensor_service_get_i2c_bus(void);
