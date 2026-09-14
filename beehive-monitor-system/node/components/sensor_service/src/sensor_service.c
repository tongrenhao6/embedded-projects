#include "sensor_service.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sht30_codec.h"

#define SHT30_INSIDE_I2C_PORT I2C_NUM_0
#define SHT30_INSIDE_SDA_GPIO GPIO_NUM_6
#define SHT30_INSIDE_SCL_GPIO GPIO_NUM_7
#define SHT30_OUTSIDE_I2C_PORT I2C_NUM_1
#define SHT30_OUTSIDE_SDA_GPIO GPIO_NUM_15
#define SHT30_OUTSIDE_SCL_GPIO GPIO_NUM_16
#define SHT30_I2C_CLOCK_HZ 100000U
#define SHT30_I2C_TIMEOUT_MS 100U
#define SHT30_SOFT_RESET_DELAY_MS 20U
#define SHT30_MEASUREMENT_DELAY_MS 30U

static const char *TAG = "sensor_service";
#define SHT30_INSIDE_ADDRESS 0x44U
#define SHT30_OUTSIDE_ADDRESS 0x44U

static i2c_master_bus_handle_t s_inside_i2c_bus;
static i2c_master_bus_handle_t s_outside_i2c_bus;
static i2c_master_dev_handle_t s_inside_sht30;
static i2c_master_dev_handle_t s_outside_sht30;

static uint16_t read_u16_be(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static esp_err_t create_i2c_bus(i2c_port_num_t port, gpio_num_t sda_gpio,
                                gpio_num_t scl_gpio,
                                i2c_master_bus_handle_t *bus)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = port,
        .sda_io_num = sda_gpio,
        .scl_io_num = scl_gpio,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    const esp_err_t result = i2c_new_master_bus(&bus_config, bus);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Create I2C%d bus on SDA=%d SCL=%d failed: %s",
                 port, sda_gpio, scl_gpio, esp_err_to_name(result));
        return result;
    }

    ESP_LOGI(TAG, "I2C%d ready on SDA=GPIO%d SCL=GPIO%d", port, sda_gpio,
             scl_gpio);
    return ESP_OK;
}

static esp_err_t add_sht30(i2c_master_bus_handle_t bus, uint8_t address,
                           const char *location,
                           i2c_master_dev_handle_t *device)
{
    esp_err_t result = i2c_master_probe(bus, address, SHT30_I2C_TIMEOUT_MS);

    if (result != ESP_OK) {
        ESP_LOGW(TAG, "SHT30 %s at 0x%02X is unavailable: %s", location,
                 address, esp_err_to_name(result));
        return result;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = SHT30_I2C_CLOCK_HZ,
    };
    result = i2c_master_bus_add_device(bus, &device_config, device);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Add SHT30 %s at 0x%02X failed: %s", location, address,
                 esp_err_to_name(result));
        return result;
    }

    ESP_LOGI(TAG, "SHT30 %s found at I2C address 0x%02X", location,
             address);
    return ESP_OK;
}

static esp_err_t reset_sht30(i2c_master_dev_handle_t device, const char *location,
                             uint8_t address)
{
    static const uint8_t soft_reset_command[] = {0x30U, 0xA2U};
    const esp_err_t result =
        i2c_master_transmit(device, soft_reset_command, sizeof(soft_reset_command),
                            SHT30_I2C_TIMEOUT_MS);

    if (result != ESP_OK) {
        ESP_LOGE(TAG, "SHT30 %s at 0x%02X soft reset failed: %s", location,
                 address, esp_err_to_name(result));
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(SHT30_SOFT_RESET_DELAY_MS));
    return ESP_OK;
}

esp_err_t sensor_service_init(void)
{
    if (s_inside_i2c_bus != NULL && s_outside_i2c_bus != NULL) {
        return ESP_OK;
    }

    if (s_inside_i2c_bus == NULL) {
        const esp_err_t result = create_i2c_bus(
            SHT30_INSIDE_I2C_PORT, SHT30_INSIDE_SDA_GPIO,
            SHT30_INSIDE_SCL_GPIO, &s_inside_i2c_bus);
        if (result != ESP_OK) {
            return result;
        }
    }

    if (s_outside_i2c_bus == NULL) {
        const esp_err_t result = create_i2c_bus(
            SHT30_OUTSIDE_I2C_PORT, SHT30_OUTSIDE_SDA_GPIO,
            SHT30_OUTSIDE_SCL_GPIO, &s_outside_i2c_bus);
        if (result != ESP_OK) {
            return result;
        }
    }

    const esp_err_t inside_result = add_sht30(
        s_inside_i2c_bus, SHT30_INSIDE_ADDRESS, "inside", &s_inside_sht30);
    if (inside_result == ESP_OK) {
        (void)reset_sht30(s_inside_sht30, "inside", SHT30_INSIDE_ADDRESS);
    }

    const esp_err_t outside_result = add_sht30(
        s_outside_i2c_bus, SHT30_OUTSIDE_ADDRESS, "outside", &s_outside_sht30);
    if (outside_result == ESP_OK) {
        (void)reset_sht30(s_outside_sht30, "outside", SHT30_OUTSIDE_ADDRESS);
    }

    return ESP_OK;
}

static esp_err_t read_sht30(i2c_master_dev_handle_t device,
                            sensor_service_sht30_reading_t *reading)
{
    static const uint8_t measure_command[] = {0x24U, 0x00U};
    uint8_t response[6] = {0};
    esp_err_t result;

    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (device == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    result = i2c_master_transmit(device, measure_command, sizeof(measure_command),
                                 SHT30_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(SHT30_MEASUREMENT_DELAY_MS));
    result = i2c_master_receive(device, response, sizeof(response),
                                SHT30_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }
    if (!sht30_crc8_matches(&response[0], response[2]) ||
        !sht30_crc8_matches(&response[3], response[5])) {
        return ESP_ERR_INVALID_CRC;
    }

    reading->temperature_centi_c =
        sht30_raw_temperature_to_centi_c(read_u16_be(&response[0]));
    reading->humidity_centi_pct =
        sht30_raw_humidity_to_centi_pct(read_u16_be(&response[3]));
    return ESP_OK;
}

esp_err_t sensor_service_read_inside_sht30(sensor_service_sht30_reading_t *reading)
{
    return read_sht30(s_inside_sht30, reading);
}

esp_err_t sensor_service_read_outside_sht30(sensor_service_sht30_reading_t *reading)
{
    return read_sht30(s_outside_sht30, reading);
}

i2c_master_bus_handle_t sensor_service_get_i2c_bus(void)
{
    return s_inside_i2c_bus;
}
