#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int32_t x_mg;
    int32_t y_mg;
    int32_t z_mg;
} tilt_sensor_accel_sample_t;

typedef struct {
    bool tilted;
    uint8_t consecutive_samples;
} tilt_sensor_state_t;

bool tilt_sensor_compute_angle_deci_degrees(
    const tilt_sensor_accel_sample_t *sample, uint16_t *angle_deci_degrees);

bool tilt_sensor_update_state(tilt_sensor_state_t *state,
                              uint16_t angle_deci_degrees);

bool tilt_sensor_is_supported_device_id(uint8_t who_am_i);
