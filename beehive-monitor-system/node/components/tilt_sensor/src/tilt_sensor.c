#include "tilt_sensor.h"

#include "esp_log.h"

#define MPU6050_ADDRESS 0x68U
#define MPU6050_COMPATIBLE_DEVICE_ID 0x70U
#define MPU6050_WHO_AM_I_REGISTER 0x75U
#define MPU6050_PWR_MGMT_1_REGISTER 0x6BU
#define MPU6050_ACCEL_CONFIG_REGISTER 0x1CU
#define MPU6050_ACCEL_XOUT_H_REGISTER 0x3BU
#define MPU6050_I2C_CLOCK_HZ 100000U
#define MPU6050_I2C_TIMEOUT_MS 100U
#define MPU6050_ACCEL_LSB_PER_G 16384

static const char *TAG = "tilt_sensor";
static i2c_master_dev_handle_t s_mpu6050;
static tilt_sensor_state_t s_tilt_state;

static int16_t read_i16_be(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static esp_err_t write_register(uint8_t register_address, uint8_t value)
{
    const uint8_t command[] = {register_address, value};

    return i2c_master_transmit(s_mpu6050, command, sizeof(command),
                               MPU6050_I2C_TIMEOUT_MS);
}

esp_err_t tilt_sensor_init(i2c_master_bus_handle_t bus)
{
    if (bus == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mpu6050 != NULL) {
        return ESP_OK;
    }

    esp_err_t result =
        i2c_master_probe(bus, MPU6050_ADDRESS, MPU6050_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 probe at 0x%02X failed: %s", MPU6050_ADDRESS,
                 esp_err_to_name(result));
        return result;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDRESS,
        .scl_speed_hz = MPU6050_I2C_CLOCK_HZ,
    };
    result = i2c_master_bus_add_device(bus, &device_config, &s_mpu6050);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Add MPU6050 at 0x%02X failed: %s", MPU6050_ADDRESS,
                 esp_err_to_name(result));
        return result;
    }

    uint8_t who_am_i = 0U;
    result = i2c_master_transmit_receive(s_mpu6050, &(uint8_t){MPU6050_WHO_AM_I_REGISTER},
                                         1U, &who_am_i, 1U,
                                         MPU6050_I2C_TIMEOUT_MS);
    if (result != ESP_OK || !tilt_sensor_is_supported_device_id(who_am_i)) {
        ESP_LOGE(TAG, "MPU6050-compatible WHO_AM_I read failed: value=0x%02X error=%s",
                 who_am_i, esp_err_to_name(result));
        return result == ESP_OK ? ESP_ERR_INVALID_RESPONSE : result;
    }

    result = write_register(MPU6050_PWR_MGMT_1_REGISTER, 0x00U);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 wake failed: %s", esp_err_to_name(result));
        return result;
    }
    result = write_register(MPU6050_ACCEL_CONFIG_REGISTER, 0x00U);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "MPU6050 accelerometer configuration failed: %s",
                 esp_err_to_name(result));
        return result;
    }

    s_tilt_state = (tilt_sensor_state_t){0};
    ESP_LOGI(TAG, "MPU6050-compatible device ready at I2C address 0x%02X (ID=0x%02X%s)",
             MPU6050_ADDRESS, who_am_i,
             who_am_i == MPU6050_COMPATIBLE_DEVICE_ID ? ", MPU-6500 compatible" : "");
    return ESP_OK;
}

esp_err_t tilt_sensor_read(tilt_sensor_reading_t *reading)
{
    uint8_t response[6] = {0};

    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mpu6050 == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    const esp_err_t result = i2c_master_transmit_receive(
        s_mpu6050, &(uint8_t){MPU6050_ACCEL_XOUT_H_REGISTER}, 1U, response,
        sizeof(response), MPU6050_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }

    reading->acceleration.x_mg =
        ((int32_t)read_i16_be(&response[0]) * 1000) / MPU6050_ACCEL_LSB_PER_G;
    reading->acceleration.y_mg =
        ((int32_t)read_i16_be(&response[2]) * 1000) / MPU6050_ACCEL_LSB_PER_G;
    reading->acceleration.z_mg =
        ((int32_t)read_i16_be(&response[4]) * 1000) / MPU6050_ACCEL_LSB_PER_G;
    if (!tilt_sensor_compute_angle_deci_degrees(&reading->acceleration,
                                                &reading->angle_deci_degrees)) {
        return ESP_ERR_INVALID_STATE;
    }

    (void)tilt_sensor_update_state(&s_tilt_state, reading->angle_deci_degrees);
    reading->tilted = s_tilt_state.tilted;
    return ESP_OK;
}
