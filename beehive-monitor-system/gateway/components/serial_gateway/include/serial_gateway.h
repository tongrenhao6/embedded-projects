#pragma once

#include <stddef.h>

#include "esp_err.h"
#include "frame_codec.h"

#define SERIAL_GATEWAY_JSON_MAX_LENGTH 256U

size_t serial_gateway_format_sensor_json(
    const frame_codec_sensor_sample_t *sample,
    char *buffer,
    size_t buffer_capacity);

esp_err_t serial_gateway_emit_sensor_json(
    const frame_codec_sensor_sample_t *sample);
