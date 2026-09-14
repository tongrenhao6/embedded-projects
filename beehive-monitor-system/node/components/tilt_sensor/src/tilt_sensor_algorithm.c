#include "tilt_sensor_algorithm.h"

#include <math.h>

#define TILT_SENSOR_TILT_CONFIRM_DECI_DEGREES 450U
#define TILT_SENSOR_RECOVER_CONFIRM_DECI_DEGREES 350U
#define TILT_SENSOR_REQUIRED_CONSECUTIVE_SAMPLES 3U
#define TILT_SENSOR_PI_F 3.14159265358979323846F

bool tilt_sensor_compute_angle_deci_degrees(
    const tilt_sensor_accel_sample_t *sample, uint16_t *angle_deci_degrees)
{
    if (sample == NULL || angle_deci_degrees == NULL) {
        return false;
    }

    const float radial_mg = hypotf((float)sample->x_mg, (float)sample->y_mg);
    const float vertical_mg = fabsf((float)sample->z_mg);
    if (hypotf(radial_mg, vertical_mg) < 1.0F) {
        return false;
    }

    const float angle = atan2f(radial_mg, vertical_mg) *
                        (1800.0F / TILT_SENSOR_PI_F);
    *angle_deci_degrees = (uint16_t)(angle + 0.5F);
    return true;
}

bool tilt_sensor_update_state(tilt_sensor_state_t *state,
                              uint16_t angle_deci_degrees)
{
    if (state == NULL) {
        return false;
    }

    const bool should_change = state->tilted
                                   ? angle_deci_degrees <=
                                         TILT_SENSOR_RECOVER_CONFIRM_DECI_DEGREES
                                   : angle_deci_degrees >=
                                         TILT_SENSOR_TILT_CONFIRM_DECI_DEGREES;
    if (!should_change) {
        state->consecutive_samples = 0U;
        return false;
    }

    if (state->consecutive_samples < TILT_SENSOR_REQUIRED_CONSECUTIVE_SAMPLES) {
        ++state->consecutive_samples;
    }
    if (state->consecutive_samples < TILT_SENSOR_REQUIRED_CONSECUTIVE_SAMPLES) {
        return false;
    }

    state->tilted = !state->tilted;
    state->consecutive_samples = 0U;
    return true;
}

bool tilt_sensor_is_supported_device_id(uint8_t who_am_i)
{
    return who_am_i == 0x68U || who_am_i == 0x70U;
}
