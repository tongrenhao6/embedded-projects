#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "frame_codec.h"

typedef void (*lora_service_frame_callback_t)(
    const frame_codec_sensor_sample_t *sample,
    void *context);

typedef struct {
    uint32_t uart_read_error_count;
    uint32_t received_byte_count;
    uint32_t valid_frame_count;
    uint32_t invalid_frame_count;
    uint32_t crc_error_count;
    uint32_t discarded_byte_count;
} lora_service_stats_t;

esp_err_t lora_service_start(lora_service_frame_callback_t callback, void *context);

void lora_service_get_stats(lora_service_stats_t *stats);
