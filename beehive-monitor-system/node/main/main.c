#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#include "board_rgb.h"
#include "esp_log.h"
#include "frame_codec.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lora_transport.h"
#include "sensor_service.h"
#include "tilt_sensor.h"

static const char *TAG = "beehive_node";

#define BEEHIVE_NODE_ID 1U

static esp_err_t send_sensor_reading(
    const sensor_service_sht30_reading_t *inside_reading,
    const sensor_service_sht30_reading_t *outside_reading, uint32_t sequence,
    uint16_t status_flags)
{
    frame_codec_sensor_sample_t sample = {
        .node_id = BEEHIVE_NODE_ID,
        .sequence = sequence,
        .uptime_ms = esp_log_timestamp(),
        .inside_temperature_centi_c = inside_reading == NULL
                                          ? 0
                                          : inside_reading->temperature_centi_c,
        .inside_humidity_centi_pct = inside_reading == NULL
                                        ? 0U
                                        : inside_reading->humidity_centi_pct,
        .outside_temperature_centi_c = outside_reading == NULL
                                           ? 0
                                           : outside_reading->temperature_centi_c,
        .outside_humidity_centi_pct = outside_reading == NULL
                                         ? 0U
                                         : outside_reading->humidity_centi_pct,
        .status_flags = status_flags,
    };
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE] = {0};
    size_t frame_length = 0U;

    if (frame_codec_encode_sensor_sample(&sample, frame, sizeof(frame),
                                         &frame_length) != FRAME_CODEC_OK) {
        return ESP_FAIL;
    }

    return lora_transport_send(frame, frame_length);
}

void app_main(void)
{
    bool sensor_service_initialized;
    ESP_LOGI(TAG, "Beehive sensor node firmware starting");
    ESP_LOGI(TAG, "Sensor frame protocol v%u, frame size %u bytes",
             FRAME_CODEC_PROTOCOL_VERSION,
             FRAME_CODEC_SENSOR_FRAME_SIZE);
    ESP_ERROR_CHECK(board_rgb_init());                  // 检测RGB LED初始化
    ESP_ERROR_CHECK(board_rgb_set_rgb(0U, 0U, 0U));     // 设置RGB LED为黑色（关闭LED灯）

    const esp_err_t lora_init_result = lora_transport_init();
    const bool lora_initialized = lora_init_result == ESP_OK;
    if (!lora_initialized) {
        ESP_LOGE(TAG, "LoRa UART initialization failed: %s",
                 esp_err_to_name(lora_init_result));
    } else {
        ESP_LOGI(TAG, "LoRa UART ready on TX=GPIO4 RX=GPIO5 at 9600 bps");
    }

    const esp_err_t sensor_init_result = sensor_service_init();
    sensor_service_initialized = sensor_init_result == ESP_OK;
    if (!sensor_service_initialized) {
        ESP_LOGE(TAG, "I2C sensor service initialization failed: %s",
                 esp_err_to_name(sensor_init_result));
    }

    const esp_err_t tilt_init_result =
        tilt_sensor_init(sensor_service_get_i2c_bus());
    const bool tilt_initialized = tilt_init_result == ESP_OK;
    if (!tilt_initialized) {
        ESP_LOGE(TAG, "MPU6050 initialization failed: %s",
                 esp_err_to_name(tilt_init_result));
    }

    uint8_t loop_count = 0U;
    uint32_t sample_sequence = 0U;
    while (true) {
        if (sensor_service_initialized && (loop_count % 2U) == 0U) {
            sensor_service_sht30_reading_t inside_reading;
            sensor_service_sht30_reading_t outside_reading;
            const esp_err_t inside_read_result =
                sensor_service_read_inside_sht30(&inside_reading);
            const esp_err_t outside_read_result =
                sensor_service_read_outside_sht30(&outside_reading);
            uint16_t status_flags = 0U;

            if (inside_read_result == ESP_OK) {
                const int temperature_whole = inside_reading.temperature_centi_c / 100;
                const int temperature_fraction =
                    abs(inside_reading.temperature_centi_c % 100);

                status_flags |= FRAME_CODEC_STATUS_INSIDE_SENSOR_VALID;
                ESP_LOGI(TAG, "SHT30 inside (0x44): temperature=%d.%02d C, humidity=%u.%02u %%",
                         temperature_whole, temperature_fraction,
                         inside_reading.humidity_centi_pct / 100U,
                         inside_reading.humidity_centi_pct % 100U);
            } else {
                ESP_LOGE(TAG, "SHT30 inside (0x44) read failed: %s",
                         esp_err_to_name(inside_read_result));
            }

            if (outside_read_result == ESP_OK) {
                const int temperature_whole = outside_reading.temperature_centi_c / 100;
                const int temperature_fraction =
                    abs(outside_reading.temperature_centi_c % 100);

                status_flags |= FRAME_CODEC_STATUS_OUTSIDE_SENSOR_VALID;
                ESP_LOGI(TAG, "SHT30 outside (0x44): temperature=%d.%02d C, humidity=%u.%02u %%",
                         temperature_whole, temperature_fraction,
                         outside_reading.humidity_centi_pct / 100U,
                         outside_reading.humidity_centi_pct % 100U);
            } else {
                ESP_LOGE(TAG, "SHT30 outside (0x44) read failed: %s",
                         esp_err_to_name(outside_read_result));
            }

                if (tilt_initialized) {
                    tilt_sensor_reading_t tilt_reading;
                    const esp_err_t tilt_read_result =
                        tilt_sensor_read(&tilt_reading);
                    if (tilt_read_result == ESP_OK) {
                        status_flags |= FRAME_CODEC_STATUS_TILT_VALID;
                        if (tilt_reading.tilted) {
                            status_flags |= FRAME_CODEC_STATUS_TILTED;
                        }
                        ESP_LOGI(TAG, "MPU6050: angle=%u.%u deg, tilted=%s",
                                 tilt_reading.angle_deci_degrees / 10U,
                                 tilt_reading.angle_deci_degrees % 10U,
                                 tilt_reading.tilted ? "yes" : "no");
                    } else {
                        ESP_LOGE(TAG, "MPU6050 read failed: %s",
                                 esp_err_to_name(tilt_read_result));
                    }
                }

                if (lora_initialized) {
                    const esp_err_t lora_send_result = send_sensor_reading(
                        inside_read_result == ESP_OK ? &inside_reading : NULL,
                        outside_read_result == ESP_OK ? &outside_reading : NULL,
                        sample_sequence, status_flags);
                    if (lora_send_result == ESP_OK) {
                        ESP_LOGI(TAG, "LoRa sent sensor frame sequence=%lu",
                                 (unsigned long)sample_sequence);
                        ++sample_sequence;
                    } else {
                        ESP_LOGE(TAG, "LoRa send failed: %s",
                                 esp_err_to_name(lora_send_result));
                    } 
                }
        }
        ++loop_count;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
