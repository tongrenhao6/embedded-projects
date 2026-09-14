#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "frame_codec.h"

#define ASSERT_TRUE(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                      \
            printf("Assertion failed: %s at %s:%d\n", #condition, __FILE__,  \
                   __LINE__);                                                   \
            return 1;                                                            \
        }                                                                        \
    } while (0)

static frame_codec_sensor_sample_t make_sample(void)
{
    return (frame_codec_sensor_sample_t){
        .node_id = 0x1201U,
        .sequence = 42U,
        .uptime_ms = 123456U,
        .inside_temperature_centi_c = 2534,
        .inside_humidity_centi_pct = 6123U,
        .outside_temperature_centi_c = -315,
        .outside_humidity_centi_pct = 7890U,
        .status_flags = 0x0003U,
    };
}

static int samples_equal(const frame_codec_sensor_sample_t *left,
                         const frame_codec_sensor_sample_t *right)
{
    return left->node_id == right->node_id &&
           left->sequence == right->sequence &&
           left->uptime_ms == right->uptime_ms &&
           left->inside_temperature_centi_c == right->inside_temperature_centi_c &&
           left->inside_humidity_centi_pct == right->inside_humidity_centi_pct &&
           left->outside_temperature_centi_c == right->outside_temperature_centi_c &&
           left->outside_humidity_centi_pct == right->outside_humidity_centi_pct &&
           left->status_flags == right->status_flags;
}

static int test_round_trip(void)
{
    const frame_codec_sensor_sample_t input = make_sample();
    frame_codec_sensor_sample_t output = {0};
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE] = {0};
    size_t encoded_length = 0;

    ASSERT_TRUE(frame_codec_encode_sensor_sample(&input, frame, sizeof(frame),
                                                  &encoded_length) == FRAME_CODEC_OK);
    ASSERT_TRUE(encoded_length == FRAME_CODEC_SENSOR_FRAME_SIZE);
    ASSERT_TRUE(frame_codec_decode_sensor_sample(frame, encoded_length, &output) ==
                FRAME_CODEC_OK);
    ASSERT_TRUE(samples_equal(&input, &output));

    return 0;
}

static int test_rejects_corrupted_crc(void)
{
    const frame_codec_sensor_sample_t input = make_sample();
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE] = {0};
    size_t encoded_length = 0;

    ASSERT_TRUE(frame_codec_encode_sensor_sample(&input, frame, sizeof(frame),
                                                  &encoded_length) == FRAME_CODEC_OK);
    frame[10] ^= 0x01U;
    ASSERT_TRUE(frame_codec_decode_sensor_sample(frame, encoded_length, &(frame_codec_sensor_sample_t){0}) ==
                FRAME_CODEC_ERR_CRC_MISMATCH);

    return 0;
}

static int test_rejects_wrong_magic(void)
{
    const frame_codec_sensor_sample_t input = make_sample();
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE] = {0};
    size_t encoded_length = 0;

    ASSERT_TRUE(frame_codec_encode_sensor_sample(&input, frame, sizeof(frame),
                                                  &encoded_length) == FRAME_CODEC_OK);
    frame[0] = 0U;
    ASSERT_TRUE(frame_codec_decode_sensor_sample(frame, encoded_length, &(frame_codec_sensor_sample_t){0}) ==
                FRAME_CODEC_ERR_INVALID_FRAME);

    return 0;
}

static int test_rejects_small_encode_buffer(void)
{
    const frame_codec_sensor_sample_t input = make_sample();
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE - 1U] = {0};
    size_t encoded_length = 0;

    ASSERT_TRUE(frame_codec_encode_sensor_sample(&input, frame, sizeof(frame),
                                                  &encoded_length) ==
                FRAME_CODEC_ERR_BUFFER_TOO_SMALL);

    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_round_trip();
    result |= test_rejects_corrupted_crc();
    result |= test_rejects_wrong_magic();
    result |= test_rejects_small_encode_buffer();

    if (result == 0) {
        printf("frame_codec tests passed\n");
    }

    return result;
}
