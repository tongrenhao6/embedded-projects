#include <stdio.h>
#include <string.h>

#include "frame_codec.h"
#include "lora_stream_parser.h"

#define ASSERT_TRUE(condition)                                                \
    do {                                                                      \
        if (!(condition)) {                                                   \
            printf("assertion failed: %s at %s:%d\n", #condition, __FILE__, \
                   __LINE__);                                                 \
            return 1;                                                         \
        }                                                                     \
    } while (0)

typedef struct {
    frame_codec_sensor_sample_t samples[4];
    size_t count;
} received_samples_t;

static frame_codec_sensor_sample_t make_sample(uint16_t node_id, uint32_t sequence)
{
    return (frame_codec_sensor_sample_t){
        .node_id = node_id,
        .sequence = sequence,
        .uptime_ms = 12000U + sequence,
        .inside_temperature_centi_c = 2567,
        .inside_humidity_centi_pct = 5345U,
        .outside_temperature_centi_c = 2475,
        .outside_humidity_centi_pct = 5678U,
        .status_flags = FRAME_CODEC_STATUS_INSIDE_SENSOR_VALID |
                        FRAME_CODEC_STATUS_OUTSIDE_SENSOR_VALID |
                        FRAME_CODEC_STATUS_TILT_VALID,
    };
}

static int encode_sample(const frame_codec_sensor_sample_t *sample,
                         uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE])
{
    size_t frame_length = 0U;
    ASSERT_TRUE(frame_codec_encode_sensor_sample(sample, frame,
                                                  FRAME_CODEC_SENSOR_FRAME_SIZE,
                                                  &frame_length) == FRAME_CODEC_OK);
    ASSERT_TRUE(frame_length == FRAME_CODEC_SENSOR_FRAME_SIZE);
    return 0;
}

static void collect_sample(const frame_codec_sensor_sample_t *sample, void *context)
{
    received_samples_t *received = context;

    if (received->count < (sizeof(received->samples) / sizeof(received->samples[0]))) {
        received->samples[received->count++] = *sample;
    }
}

static int test_decodes_a_valid_frame(void)
{
    lora_stream_parser_t parser;
    received_samples_t received = {0};
    const frame_codec_sensor_sample_t input = make_sample(17U, 9U);
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE] = {0};

    ASSERT_TRUE(encode_sample(&input, frame) == 0);
    lora_stream_parser_init(&parser);
    lora_stream_parser_push(&parser, frame, sizeof(frame), collect_sample, &received);

    ASSERT_TRUE(received.count == 1U);
    ASSERT_TRUE(received.samples[0].node_id == input.node_id);
    ASSERT_TRUE(received.samples[0].sequence == input.sequence);
    ASSERT_TRUE(lora_stream_parser_valid_frame_count(&parser) == 1U);
    return 0;
}

static int test_skips_noise_before_a_valid_frame(void)
{
    lora_stream_parser_t parser;
    received_samples_t received = {0};
    const frame_codec_sensor_sample_t input = make_sample(18U, 10U);
    uint8_t bytes[3U + FRAME_CODEC_SENSOR_FRAME_SIZE] = {0x00U, 0x42U, 0xB0U};

    ASSERT_TRUE(encode_sample(&input, &bytes[3]) == 0);
    lora_stream_parser_init(&parser);
    lora_stream_parser_push(&parser, bytes, sizeof(bytes), collect_sample, &received);

    ASSERT_TRUE(received.count == 1U);
    ASSERT_TRUE(received.samples[0].sequence == input.sequence);
    ASSERT_TRUE(lora_stream_parser_discarded_byte_count(&parser) == 3U);
    return 0;
}

static int test_recovers_after_a_crc_damaged_frame(void)
{
    lora_stream_parser_t parser;
    received_samples_t received = {0};
    const frame_codec_sensor_sample_t input = make_sample(19U, 11U);
    uint8_t bytes[FRAME_CODEC_SENSOR_FRAME_SIZE * 2U] = {0};

    ASSERT_TRUE(encode_sample(&input, bytes) == 0);
    bytes[24] ^= 0x01U;
    ASSERT_TRUE(encode_sample(&input, &bytes[FRAME_CODEC_SENSOR_FRAME_SIZE]) == 0);
    lora_stream_parser_init(&parser);
    lora_stream_parser_push(&parser, bytes, sizeof(bytes), collect_sample, &received);

    ASSERT_TRUE(received.count == 1U);
    ASSERT_TRUE(lora_stream_parser_crc_error_count(&parser) == 1U);
    ASSERT_TRUE(lora_stream_parser_valid_frame_count(&parser) == 1U);
    return 0;
}

static int test_recovers_after_a_format_damaged_frame(void)
{
    lora_stream_parser_t parser;
    received_samples_t received = {0};
    const frame_codec_sensor_sample_t input = make_sample(20U, 12U);
    uint8_t bytes[FRAME_CODEC_SENSOR_FRAME_SIZE * 2U] = {0};

    ASSERT_TRUE(encode_sample(&input, bytes) == 0);
    bytes[1] = 0x02U;
    ASSERT_TRUE(encode_sample(&input, &bytes[FRAME_CODEC_SENSOR_FRAME_SIZE]) == 0);
    lora_stream_parser_init(&parser);
    lora_stream_parser_push(&parser, bytes, sizeof(bytes), collect_sample, &received);

    ASSERT_TRUE(received.count == 1U);
    ASSERT_TRUE(lora_stream_parser_invalid_frame_count(&parser) == 1U);
    return 0;
}

static int test_waits_for_a_frame_split_across_reads(void)
{
    lora_stream_parser_t parser;
    received_samples_t received = {0};
    const frame_codec_sensor_sample_t input = make_sample(21U, 13U);
    uint8_t frame[FRAME_CODEC_SENSOR_FRAME_SIZE] = {0};

    ASSERT_TRUE(encode_sample(&input, frame) == 0);
    lora_stream_parser_init(&parser);
    lora_stream_parser_push(&parser, frame, 7U, collect_sample, &received);
    ASSERT_TRUE(received.count == 0U);
    lora_stream_parser_push(&parser, &frame[7], sizeof(frame) - 7U, collect_sample,
                            &received);

    ASSERT_TRUE(received.count == 1U);
    ASSERT_TRUE(received.samples[0].sequence == input.sequence);
    return 0;
}

static int test_decodes_two_consecutive_frames(void)
{
    lora_stream_parser_t parser;
    received_samples_t received = {0};
    const frame_codec_sensor_sample_t first = make_sample(22U, 14U);
    const frame_codec_sensor_sample_t second = make_sample(23U, 15U);
    uint8_t bytes[FRAME_CODEC_SENSOR_FRAME_SIZE * 2U] = {0};

    ASSERT_TRUE(encode_sample(&first, bytes) == 0);
    ASSERT_TRUE(encode_sample(&second, &bytes[FRAME_CODEC_SENSOR_FRAME_SIZE]) == 0);
    lora_stream_parser_init(&parser);
    lora_stream_parser_push(&parser, bytes, sizeof(bytes), collect_sample, &received);

    ASSERT_TRUE(received.count == 2U);
    ASSERT_TRUE(received.samples[0].sequence == first.sequence);
    ASSERT_TRUE(received.samples[1].sequence == second.sequence);
    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_decodes_a_valid_frame();
    result |= test_skips_noise_before_a_valid_frame();
    result |= test_recovers_after_a_crc_damaged_frame();
    result |= test_recovers_after_a_format_damaged_frame();
    result |= test_waits_for_a_frame_split_across_reads();
    result |= test_decodes_two_consecutive_frames();

    if (result == 0) {
        printf("lora_stream_parser tests passed\n");
    }
    return result;
}
