#include <inttypes.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "board_info.h"
#include "board_rgb.h"
#include "esp_log.h"
#include "frame_codec.h"
#include "lora_service.h"
#include "mqtt_service.h"
#include "serial_gateway.h"
#include "wifi_service.h"

static const char *TAG = "beehive_app";

static void log_temperature_humidity(const char *location,
                                     int16_t temperature_centi_c,
                                     uint16_t humidity_centi_pct,
                                     bool valid)
{
    if (!valid) {
        ESP_LOGI(TAG, "%s temperature/humidity: invalid", location);
        return;
    }

    const int32_t temperature = temperature_centi_c;
    const int32_t absolute_temperature =
        temperature < 0 ? -temperature : temperature;
    ESP_LOGI(TAG, "%s: temperature=%s%" PRId32 ".%02" PRId32
                  " C, humidity=%" PRIu32 ".%02" PRIu32 " %%",
             location, temperature < 0 ? "-" : "", absolute_temperature / 100,
             absolute_temperature % 100,
             (uint32_t)humidity_centi_pct / 100U,
             (uint32_t)humidity_centi_pct % 100U);
}

static void log_received_sensor_sample(const frame_codec_sensor_sample_t *sample,
                                       void *context)
{
    (void)context;
    const bool inside_valid =
        (sample->status_flags & FRAME_CODEC_STATUS_INSIDE_SENSOR_VALID) != 0U;
    const bool outside_valid =
        (sample->status_flags & FRAME_CODEC_STATUS_OUTSIDE_SENSOR_VALID) != 0U;
    const bool tilt_valid =
        (sample->status_flags & FRAME_CODEC_STATUS_TILT_VALID) != 0U;

    ESP_LOGI(TAG, "LoRa frame: node_id=%u, sequence=%" PRIu32
                  ", uptime_ms=%" PRIu32 ", status_flags=0x%04X",
             sample->node_id, sample->sequence, sample->uptime_ms,
             sample->status_flags);
    log_temperature_humidity("Inside", sample->inside_temperature_centi_c,
                             sample->inside_humidity_centi_pct, inside_valid);
    log_temperature_humidity("Outside", sample->outside_temperature_centi_c,
                             sample->outside_humidity_centi_pct, outside_valid);
    ESP_LOGI(TAG, "Tilt: %s", tilt_valid
                                   ? ((sample->status_flags &
                                       FRAME_CODEC_STATUS_TILTED) != 0U
                                          ? "tilted=yes"
                                          : "tilted=no")
                                   : "invalid");

    const esp_err_t serial_result = serial_gateway_emit_sensor_json(sample);
    if (serial_result != ESP_OK) {
        ESP_LOGW(TAG, "Serial JSON output failed: %s",
                 esp_err_to_name(serial_result));
    }

    const esp_err_t mqtt_result = mqtt_service_enqueue_sensor_sample(sample);
    if (mqtt_result != ESP_OK && mqtt_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "MQTT telemetry enqueue failed: %s",
                 esp_err_to_name(mqtt_result));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Beehive LoRa gateway firmware starting");
    ESP_LOGI(TAG, "Sensor frame protocol v%u, frame size %u bytes",
             FRAME_CODEC_PROTOCOL_VERSION,
             FRAME_CODEC_SENSOR_FRAME_SIZE);
    ESP_LOGI(TAG, "Device role: %s, gateway id: %" PRIu32,
             APP_DEVICE_ROLE_NAME,
             (uint32_t)APP_GATEWAY_ID);

    board_info_log();

    esp_err_t rgb_result = board_rgb_init();
    if (rgb_result == ESP_OK) {
        rgb_result = board_rgb_set_rgb(0U, 0U, 0U);
    }
    if (rgb_result != ESP_OK) {
        ESP_LOGW(TAG, "RGB LED clear failed: %s", esp_err_to_name(rgb_result));
    }

    const esp_err_t wifi_result = wifi_service_start();
    if (wifi_result != ESP_OK && wifi_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Wi-Fi STA failed to start: %s",
                 esp_err_to_name(wifi_result));
    }

    const esp_err_t mqtt_result = mqtt_service_start();
    if (mqtt_result != ESP_OK && mqtt_result != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "MQTT service failed to start: %s",
                 esp_err_to_name(mqtt_result));
    }

    const esp_err_t lora_result =
        lora_service_start(log_received_sensor_sample, NULL);
    if (lora_result != ESP_OK) {
        ESP_LOGE(TAG, "LoRa UART receiver failed to start: %s",
                 esp_err_to_name(lora_result));
    }

    uint32_t heartbeat = 0;
    while (true) {
        mqtt_service_stats_t mqtt_stats;
        mqtt_service_get_stats(&mqtt_stats);
        ESP_LOGI(TAG,
                 "Gateway heartbeat #%" PRIu32 ", waiting for LoRa frames, "
                 "wifi=%s, mqtt=%s, mqtt_ok=%" PRIu32
                 ", mqtt_fail=%" PRIu32 ", mqtt_drop=%" PRIu32,
                 ++heartbeat, wifi_service_is_connected() ? "yes" : "no",
                 mqtt_service_is_connected() ? "yes" : "no",
                 mqtt_stats.telemetry_publish_success_count,
                 mqtt_stats.telemetry_publish_failure_count,
                 mqtt_stats.queue_drop_count);
        vTaskDelay(pdMS_TO_TICKS(APP_HEARTBEAT_PERIOD_MS));
    }
}
