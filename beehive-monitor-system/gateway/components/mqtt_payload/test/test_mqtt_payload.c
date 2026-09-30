#include <stdio.h>
#include <string.h>

#include "frame_codec.h"
#include "mqtt_payload.h"

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
        .sequence = 11U,
        .uptime_ms = 23906U,
        .inside_temperature_centi_c = 2735,
        .inside_humidity_centi_pct = 4782U,
        .outside_temperature_centi_c = 2664,
        .outside_humidity_centi_pct = 5200U,
        .status_flags = status_flags,
    };
}

static int test_formats_gateway_id_and_topics(void)
{
    char gateway_id[MQTT_PAYLOAD_GATEWAY_ID_MAX_LENGTH] = {0};
    char topic[MQTT_PAYLOAD_TOPIC_MAX_LENGTH] = {0};

    ASSERT_TRUE(mqtt_payload_format_gateway_id(1U, gateway_id,
                                               sizeof(gateway_id)) > 0U);
    ASSERT_TRUE(strcmp(gateway_id, "gw-001") == 0);
    ASSERT_TRUE(mqtt_payload_format_telemetry_topic(
                    "beehive/v1", gateway_id, 1U, topic, sizeof(topic)) > 0U);
    ASSERT_TRUE(strcmp(topic,
                       "beehive/v1/gateways/gw-001/nodes/1/telemetry") == 0);
    ASSERT_TRUE(mqtt_payload_format_status_topic(
                    "beehive/v1", gateway_id, topic, sizeof(topic)) > 0U);
    ASSERT_TRUE(strcmp(topic, "beehive/v1/gateways/gw-001/status") == 0);
    return 0;
}

static int test_formats_valid_telemetry_json(void)
{
    char json[MQTT_PAYLOAD_JSON_MAX_LENGTH] = {0};
    const frame_codec_sensor_sample_t sample = make_sample(
        FRAME_CODEC_STATUS_INSIDE_SENSOR_VALID |
        FRAME_CODEC_STATUS_OUTSIDE_SENSOR_VALID |
        FRAME_CODEC_STATUS_TILT_VALID |
        FRAME_CODEC_STATUS_TILTED);

    ASSERT_TRUE(mqtt_payload_format_telemetry_json(
                    "gw-001", &sample, json, sizeof(json)) > 0U);
    ASSERT_TRUE(strstr(json, "\"schema_version\":1") != NULL);
    ASSERT_TRUE(strstr(json, "\"gateway_id\":\"gw-001\"") != NULL);
    ASSERT_TRUE(strstr(json, "\"sequence\":11") != NULL);
    ASSERT_TRUE(strstr(json, "\"uptime_ms\":23906") != NULL);
    ASSERT_TRUE(strstr(json, "\"inside_temperature\":27.35") != NULL);
    ASSERT_TRUE(strstr(json, "\"inside_humidity\":47.82") != NULL);
    ASSERT_TRUE(strstr(json, "\"outside_temperature\":26.64") != NULL);
    ASSERT_TRUE(strstr(json, "\"outside_humidity\":52.00") != NULL);
    ASSERT_TRUE(strstr(json, "\"tilted\":true") != NULL);
    ASSERT_TRUE(strstr(json, "\"status_flags\":15") != NULL);
    return 0;
}

static int test_formats_invalid_measurements_as_null(void)
{
    char json[MQTT_PAYLOAD_JSON_MAX_LENGTH] = {0};
    const frame_codec_sensor_sample_t sample = make_sample(
        FRAME_CODEC_STATUS_TILT_VALID);

    ASSERT_TRUE(mqtt_payload_format_telemetry_json(
                    "gw-001", &sample, json, sizeof(json)) > 0U);
    ASSERT_TRUE(strstr(json, "\"inside_temperature\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"inside_humidity\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"outside_temperature\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"outside_humidity\":null") != NULL);
    ASSERT_TRUE(strstr(json, "\"tilted\":false") != NULL);
    ASSERT_TRUE(strstr(json, "\"status_flags\":4") != NULL);
    return 0;
}

static int test_rejects_invalid_arguments_and_small_buffers(void)
{
    char buffer[6] = {0};
    const frame_codec_sensor_sample_t sample = make_sample(0U);

    ASSERT_TRUE(mqtt_payload_format_gateway_id(1U, buffer, sizeof(buffer)) == 0U);
    ASSERT_TRUE(mqtt_payload_format_telemetry_json(
                    "gw-001", &sample, buffer, sizeof(buffer)) == 0U);
    ASSERT_TRUE(mqtt_payload_format_telemetry_json(
                    NULL, &sample, buffer, sizeof(buffer)) == 0U);
    return 0;
}

static int test_formats_distinct_topics_and_payloads_for_consecutive_nodes(void)
{
    char first_topic[MQTT_PAYLOAD_TOPIC_MAX_LENGTH] = {0};
    char second_topic[MQTT_PAYLOAD_TOPIC_MAX_LENGTH] = {0};
    char first_json[MQTT_PAYLOAD_JSON_MAX_LENGTH] = {0};
    char second_json[MQTT_PAYLOAD_JSON_MAX_LENGTH] = {0};
    const frame_codec_sensor_sample_t first_sample = make_sample(0x0007U);
    frame_codec_sensor_sample_t second_sample = first_sample;
    second_sample.node_id = 2U;
    second_sample.sequence = 12U;

    ASSERT_TRUE(mqtt_payload_format_telemetry_topic(
                    "beehive/v1", "gw-001", first_sample.node_id, first_topic,
                    sizeof(first_topic)) > 0U);
    ASSERT_TRUE(mqtt_payload_format_telemetry_topic(
                    "beehive/v1", "gw-001", second_sample.node_id, second_topic,
                    sizeof(second_topic)) > 0U);
    ASSERT_TRUE(strcmp(first_topic,
                       "beehive/v1/gateways/gw-001/nodes/1/telemetry") == 0);
    ASSERT_TRUE(strcmp(second_topic,
                       "beehive/v1/gateways/gw-001/nodes/2/telemetry") == 0);
    ASSERT_TRUE(mqtt_payload_format_telemetry_json(
                    "gw-001", &first_sample, first_json, sizeof(first_json)) > 0U);
    ASSERT_TRUE(mqtt_payload_format_telemetry_json(
                    "gw-001", &second_sample, second_json, sizeof(second_json)) > 0U);
    ASSERT_TRUE(strstr(first_json, "\"node_id\":1") != NULL);
    ASSERT_TRUE(strstr(first_json, "\"sequence\":11") != NULL);
    ASSERT_TRUE(strstr(second_json, "\"node_id\":2") != NULL);
    ASSERT_TRUE(strstr(second_json, "\"sequence\":12") != NULL);
    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_formats_gateway_id_and_topics();
    result |= test_formats_valid_telemetry_json();
    result |= test_formats_invalid_measurements_as_null();
    result |= test_rejects_invalid_arguments_and_small_buffers();
    result |= test_formats_distinct_topics_and_payloads_for_consecutive_nodes();

    if (result == 0) {
        printf("mqtt_payload tests passed\n");
    }
    return result;
}
