#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "frame_codec.h"

typedef struct {
    uint32_t connected_count;
    uint32_t disconnected_count;
    uint32_t telemetry_publish_success_count;
    uint32_t telemetry_publish_failure_count;
    uint32_t queue_drop_count;
} mqtt_service_stats_t;

esp_err_t mqtt_service_start(void);

esp_err_t mqtt_service_enqueue_sensor_sample(
    const frame_codec_sensor_sample_t *sample);

bool mqtt_service_is_connected(void);

void mqtt_service_get_stats(mqtt_service_stats_t *stats);
