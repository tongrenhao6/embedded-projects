#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#include "tilt_sensor_algorithm.h"

typedef struct {
    tilt_sensor_accel_sample_t acceleration;
    uint16_t angle_deci_degrees;
    bool tilted;
} tilt_sensor_reading_t;

esp_err_t tilt_sensor_init(i2c_master_bus_handle_t bus);
esp_err_t tilt_sensor_read(tilt_sensor_reading_t *reading);
