#pragma once

#include <stddef.h>
#include <stdint.h>

#include "frame_codec.h"

#define MQTT_PAYLOAD_GATEWAY_ID_MAX_LENGTH 32U
#define MQTT_PAYLOAD_TOPIC_MAX_LENGTH 128U
#define MQTT_PAYLOAD_JSON_MAX_LENGTH 512U

size_t mqtt_payload_format_gateway_id(uint32_t gateway_id,
                                      char *buffer,
                                      size_t buffer_capacity);

size_t mqtt_payload_format_telemetry_topic(const char *topic_prefix,
                                           const char *gateway_id,
                                           uint16_t node_id,
                                           char *buffer,
                                           size_t buffer_capacity);

size_t mqtt_payload_format_status_topic(const char *topic_prefix,
                                        const char *gateway_id,
                                        char *buffer,
                                        size_t buffer_capacity);

size_t mqtt_payload_copy_topic(const char *topic, char *buffer, size_t buffer_capacity);

size_t mqtt_payload_format_telemetry_json(
    const char *gateway_id,
    const frame_codec_sensor_sample_t *sample,
    char *buffer,
    size_t buffer_capacity);
