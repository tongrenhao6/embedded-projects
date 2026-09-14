#include <stdint.h>
#include <stdio.h>

#include "tilt_sensor_algorithm.h"

#define ASSERT_TRUE(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                      \
            printf("Assertion failed: %s at %s:%d\n", #condition, __FILE__,  \
                   __LINE__);                                                   \
            return 1;                                                            \
        }                                                                        \
    } while (0)

static int test_level_box_has_zero_tilt(void)
{
    uint16_t angle_deci_degrees = UINT16_MAX;

    ASSERT_TRUE(tilt_sensor_compute_angle_deci_degrees(
        &(tilt_sensor_accel_sample_t){.x_mg = 0, .y_mg = 0, .z_mg = 1000},
        &angle_deci_degrees));
    ASSERT_TRUE(angle_deci_degrees == 0U);
    return 0;
}

static int test_sideways_box_has_ninety_degree_tilt(void)
{
    uint16_t angle_deci_degrees = 0U;

    ASSERT_TRUE(tilt_sensor_compute_angle_deci_degrees(
        &(tilt_sensor_accel_sample_t){.x_mg = 1000, .y_mg = 0, .z_mg = 0},
        &angle_deci_degrees));
    ASSERT_TRUE(angle_deci_degrees >= 895U && angle_deci_degrees <= 905U);
    return 0;
}

static int test_rejects_zero_gravity_vector(void)
{
    uint16_t angle_deci_degrees = 0U;

    ASSERT_TRUE(!tilt_sensor_compute_angle_deci_degrees(
        &(tilt_sensor_accel_sample_t){0}, &angle_deci_degrees));
    return 0;
}

static int test_uses_confirmation_and_recovery_hysteresis(void)
{
    tilt_sensor_state_t state = {0};

    ASSERT_TRUE(!tilt_sensor_update_state(&state, 460U));
    ASSERT_TRUE(!tilt_sensor_update_state(&state, 460U));
    ASSERT_TRUE(tilt_sensor_update_state(&state, 460U));
    ASSERT_TRUE(state.tilted);

    ASSERT_TRUE(!tilt_sensor_update_state(&state, 340U));
    ASSERT_TRUE(!tilt_sensor_update_state(&state, 340U));
    ASSERT_TRUE(tilt_sensor_update_state(&state, 340U));
    ASSERT_TRUE(!state.tilted);
    return 0;
}

static int test_accepts_mpu6050_compatible_device_ids(void)
{
    ASSERT_TRUE(tilt_sensor_is_supported_device_id(0x68U));
    ASSERT_TRUE(tilt_sensor_is_supported_device_id(0x70U));
    ASSERT_TRUE(!tilt_sensor_is_supported_device_id(0x00U));
    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_level_box_has_zero_tilt();
    result |= test_sideways_box_has_ninety_degree_tilt();
    result |= test_rejects_zero_gravity_vector();
    result |= test_uses_confirmation_and_recovery_hysteresis();
    result |= test_accepts_mpu6050_compatible_device_ids();

    if (result == 0) {
        printf("tilt_sensor tests passed\n");
    }
    return result;
}
