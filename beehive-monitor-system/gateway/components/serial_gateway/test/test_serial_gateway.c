#include <stdio.h>
#include <string.h>

#include "frame_codec.h"
#include "serial_gateway.h"

#define ASSERT_TRUE(condition)                                                \
    do {                                                                      \
        if (!(condition)) {                                                   \
            printf("assertion failed: %s at %s:%d\n", #condition, __FILE__, \
                   __LINE__);                                                 \
            return 1;                                                         \
        }                                                                     \
    } while (0)

static frame_codec_sensor_sample_t make_sample(uint16_t status_flags)
{
    return (frame_codec_sensor_sample_t){
        .node_id = 1U,
        .sequence = 9U,
        .uptime_ms = 916U,
        .inside_temperature_centi_c = 2580,
        .inside_humidity_centi_pct = 4824U,
        .outside_temperature_centi_c = 2587,
        .outside_humidity_centi_pct = 4973U,
        .status_flags = status_flags,
    };
}

static int test_formats_a_complete_sample(void)
{
    char json[SERIAL_GATEWAY_JSON_MAX_LENGTH] = {0};
    const frame_codec_sensor_sample_t sample = make_sample(0x0007U);

    ASSERT_TRUE(serial_gateway_format_sensor_json(&sample, json, sizeof(json)) > 0U);
    ASSERT_TRUE(strcmp(json,
                      "GWJSON {\"node_id\":1,\"sequence\":9,\"uptime_ms\":916,"
                      "\"inside_temperature\":25.80,\"inside_humidity\":48.24,"
                      "\"outside_temperature\":25.87,\"outside_humidity\":49.73,"
                      "\"tilted\":false,\"status_flags\":7}\n") == 0);
    return 0;
}

static int test_marks_invalid_measurements_as_null(void)
{
    char json[SERIAL_GATEWAY_JSON_MAX_LENGTH] = {0};
    const frame_codec_sensor_sample_t sample = make_sample(
        FRAME_CODEC_STATUS_TILT_VALID | FRAME_CODEC_STATUS_TILTED);

    ASSERT_TRUE(serial_gateway_format_sensor_json(&sample, json, sizeof(json)) > 0U);
    ASSERT_TRUE(strstr(json, "\"inside_temperature\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"inside_humidity\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"outside_temperature\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"outside_humidity\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"tilted\":true") != NULL);
    return 0;
}

static int test_rejects_small_output_buffer(void)
{
    char json[8] = {0};
    const frame_codec_sensor_sample_t sample = make_sample(0x0007U);

    ASSERT_TRUE(serial_gateway_format_sensor_json(&sample, json, sizeof(json)) == 0U);
    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_formats_a_complete_sample();
    result |= test_marks_invalid_measurements_as_null();
    result |= test_rejects_small_output_buffer();

    if (result == 0) {
        printf("serial_gateway tests passed\n");
    }
    return result;
}
