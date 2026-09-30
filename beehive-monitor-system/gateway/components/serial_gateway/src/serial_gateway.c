#include "serial_gateway.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>

static void format_signed_centi(int16_t value, char *buffer, size_t capacity)
{
    const int32_t signed_value = value;
    const int32_t absolute_value = signed_value < 0 ? -signed_value : signed_value;

    snprintf(buffer, capacity, "%s%" PRId32 ".%02" PRId32,
             signed_value < 0 ? "-" : "", absolute_value / 100,
             absolute_value % 100);
}

static void format_unsigned_centi(uint16_t value, char *buffer, size_t capacity)
{
    snprintf(buffer, capacity, "%" PRIu32 ".%02" PRIu32,
             (uint32_t)value / 100U, (uint32_t)value % 100U);
}

size_t serial_gateway_format_sensor_json(
    const frame_codec_sensor_sample_t *sample,
    char *buffer,
    size_t buffer_capacity)
{
    if (sample == NULL || buffer == NULL || buffer_capacity == 0U) {
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

    if (inside_valid) {
        format_signed_centi(sample->inside_temperature_centi_c, inside_temperature,
                            sizeof(inside_temperature));
        format_unsigned_centi(sample->inside_humidity_centi_pct, inside_humidity,
                              sizeof(inside_humidity));
    }
    if (outside_valid) {
        format_signed_centi(sample->outside_temperature_centi_c,
                            outside_temperature, sizeof(outside_temperature));
        format_unsigned_centi(sample->outside_humidity_centi_pct, outside_humidity,
                              sizeof(outside_humidity));
    }

    const char *tilted = "null";
    if (tilt_valid) {
        tilted = (status_flags & FRAME_CODEC_STATUS_TILTED) != 0U ? "true" : "false";
    }

    const int written = snprintf(
        buffer, buffer_capacity,
        "GWJSON {\"node_id\":%u,\"sequence\":%" PRIu32
        ",\"uptime_ms\":%" PRIu32 ",\"inside_temperature\":%s"
        ",\"inside_humidity\":%s,\"outside_temperature\":%s"
        ",\"outside_humidity\":%s,\"tilted\":%s,\"status_flags\":%u}\n",
        (unsigned)sample->node_id, sample->sequence, sample->uptime_ms,
        inside_temperature, inside_humidity, outside_temperature, outside_humidity,
        tilted, (unsigned)status_flags);

    if (written < 0 || (size_t)written >= buffer_capacity) {
        buffer[0] = '\0';
        return 0U;
    }
    return (size_t)written;
}

esp_err_t serial_gateway_emit_sensor_json(
    const frame_codec_sensor_sample_t *sample)
{
    char json[SERIAL_GATEWAY_JSON_MAX_LENGTH];
    const size_t length = serial_gateway_format_sensor_json(
        sample, json, sizeof(json));
    if (length == 0U) {
        return ESP_ERR_INVALID_ARG;
    }
    if (fputs(json, stdout) == EOF || fflush(stdout) != 0) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
