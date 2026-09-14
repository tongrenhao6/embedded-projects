#include "mqtt_payload.h"

#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool format_signed_centi(int16_t value, char *buffer, size_t capacity)
{
    const int32_t signed_value = value;
    const int32_t absolute_value = signed_value < 0 ? -signed_value : signed_value;
    const int written = snprintf(buffer, capacity, "%s%" PRId32 ".%02" PRId32,
                                 signed_value < 0 ? "-" : "",
                                 absolute_value / 100, absolute_value % 100);
    return written >= 0 && (size_t)written < capacity;
}

static bool format_unsigned_centi(uint16_t value, char *buffer, size_t capacity)
{
    const int written = snprintf(buffer, capacity, "%" PRIu32 ".%02" PRIu32,
                                 (uint32_t)value / 100U,
                                 (uint32_t)value % 100U);
    return written >= 0 && (size_t)written < capacity;
}

static size_t finish_snprintf(int written, char *buffer, size_t capacity)
{
    if (written < 0 || (size_t)written >= capacity) {
        if (buffer != NULL && capacity > 0U) {
            buffer[0] = '\0';
        }
        return 0U;
    }
    return (size_t)written;
}

static size_t trim_topic_prefix(const char *topic_prefix)
{
    size_t length = topic_prefix == NULL ? 0U : strlen(topic_prefix);
    while (length > 0U && topic_prefix[length - 1U] == '/') {
        --length;
    }
    return length;
}

size_t mqtt_payload_format_gateway_id(uint32_t gateway_id,
                                      char *buffer,
                                      size_t buffer_capacity)
{
    if (buffer == NULL || buffer_capacity == 0U) {
        return 0U;
    }

    return finish_snprintf(snprintf(buffer, buffer_capacity, "gw-%03" PRIu32,
                                    gateway_id),
                           buffer, buffer_capacity);
}

size_t mqtt_payload_format_telemetry_topic(const char *topic_prefix,
                                           const char *gateway_id,
                                           uint16_t node_id,
                                           char *buffer,
                                           size_t buffer_capacity)
{
    const size_t prefix_length = trim_topic_prefix(topic_prefix);
    if (prefix_length == 0U || gateway_id == NULL || gateway_id[0] == '\0' ||
        buffer == NULL || buffer_capacity == 0U || prefix_length > (size_t)INT_MAX) {
        return 0U;
    }

    return finish_snprintf(
        snprintf(buffer, buffer_capacity, "%.*s/gateways/%s/nodes/%u/telemetry",
                 (int)prefix_length, topic_prefix, gateway_id, (unsigned)node_id),
        buffer, buffer_capacity);
}

size_t mqtt_payload_format_status_topic(const char *topic_prefix,
                                        const char *gateway_id,
                                        char *buffer,
                                        size_t buffer_capacity)
{
    const size_t prefix_length = trim_topic_prefix(topic_prefix);
    if (prefix_length == 0U || gateway_id == NULL || gateway_id[0] == '\0' ||
        buffer == NULL || buffer_capacity == 0U || prefix_length > (size_t)INT_MAX) {
        return 0U;
    }

    return finish_snprintf(snprintf(buffer, buffer_capacity, "%.*s/gateways/%s/status",
                                    (int)prefix_length, topic_prefix, gateway_id),
                           buffer, buffer_capacity);
}

size_t mqtt_payload_copy_topic(const char *topic, char *buffer, size_t buffer_capacity)
{
    if (topic == NULL || topic[0] == '\0' || buffer == NULL || buffer_capacity == 0U) {
        return 0U;
    }
    return finish_snprintf(snprintf(buffer, buffer_capacity, "%s", topic), buffer, buffer_capacity);
}

size_t mqtt_payload_format_telemetry_json(
    const char *gateway_id,
    const frame_codec_sensor_sample_t *sample,
    char *buffer,
    size_t buffer_capacity)
{
    if (gateway_id == NULL || gateway_id[0] == '\0' || sample == NULL ||
        buffer == NULL || buffer_capacity == 0U) {
        return 0U;
    }

    char inside_temperature[24] = "null";
    char inside_humidity[24] = "null";
    char outside_temperature[24] = "null";
    char outside_humidity[24] = "null";
    const uint16_t status_flags = sample->status_flags;
    const bool inside_valid =
        (status_flags & FRAME_CODEC_STATUS_INSIDE_SENSOR_VALID) != 0U;
    const bool outside_valid =
        (status_flags & FRAME_CODEC_STATUS_OUTSIDE_SENSOR_VALID) != 0U;
    const bool tilt_valid =
        (status_flags & FRAME_CODEC_STATUS_TILT_VALID) != 0U;

    if (inside_valid &&
        (!format_signed_centi(sample->inside_temperature_centi_c,
                              inside_temperature, sizeof(inside_temperature)) ||
         !format_unsigned_centi(sample->inside_humidity_centi_pct,
                                inside_humidity, sizeof(inside_humidity)))) {
        return 0U;
    }
    if (outside_valid &&
        (!format_signed_centi(sample->outside_temperature_centi_c,
                              outside_temperature, sizeof(outside_temperature)) ||
         !format_unsigned_centi(sample->outside_humidity_centi_pct,
                                outside_humidity, sizeof(outside_humidity)))) {
        return 0U;
    }

    const char *tilted = "null";
    if (tilt_valid) {
        tilted = (status_flags & FRAME_CODEC_STATUS_TILTED) != 0U ? "true" : "false";
    }

    return finish_snprintf(
        snprintf(buffer, buffer_capacity,
                 "{\"schema_version\":1,\"gateway_id\":\"%s\","
                 "\"node_id\":%u,\"sequence\":%" PRIu32
                 ",\"uptime_ms\":%" PRIu32
                 ",\"inside_temperature\":%s,\"inside_humidity\":%s"
                 ",\"outside_temperature\":%s,\"outside_humidity\":%s"
                 ",\"tilted\":%s,\"status_flags\":%u}",
                 gateway_id, (unsigned)sample->node_id, sample->sequence,
                 sample->uptime_ms, inside_temperature, inside_humidity,
                 outside_temperature, outside_humidity, tilted,
                 (unsigned)status_flags),
        buffer, buffer_capacity);
}
