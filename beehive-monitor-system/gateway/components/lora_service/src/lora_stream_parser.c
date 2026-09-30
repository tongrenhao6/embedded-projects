#include "lora_stream_parser.h"

#include <string.h>

static void lora_stream_parser_resynchronize(lora_stream_parser_t *parser)
{
    size_t next_start = FRAME_CODEC_SENSOR_FRAME_SIZE;

    for (size_t index = 1U; index < FRAME_CODEC_SENSOR_FRAME_SIZE; ++index) {
        if (parser->buffered_frame[index] == LORA_STREAM_PARSER_FRAME_MAGIC) {
            next_start = index;
            break;
        }
    }

    parser->discarded_byte_count += (uint32_t)next_start;
    if (next_start == FRAME_CODEC_SENSOR_FRAME_SIZE) {
        parser->buffered_length = 0U;
        return;
    }

    parser->buffered_length = FRAME_CODEC_SENSOR_FRAME_SIZE - next_start;
    memmove(parser->buffered_frame, &parser->buffered_frame[next_start],
            parser->buffered_length);
}

void lora_stream_parser_init(lora_stream_parser_t *parser)
{
    if (parser != NULL) {
        memset(parser, 0, sizeof(*parser));
    }
}

void lora_stream_parser_push(lora_stream_parser_t *parser,
                             const uint8_t *data,
                             size_t data_length,
                             lora_stream_parser_frame_callback_t callback,
                             void *callback_context)
{
    if (parser == NULL || data == NULL || callback == NULL) {
        return;
    }

    for (size_t index = 0U; index < data_length; ++index) {
        if (parser->buffered_length == 0U &&
            data[index] != LORA_STREAM_PARSER_FRAME_MAGIC) {
            ++parser->discarded_byte_count;
            continue;
        }

        parser->buffered_frame[parser->buffered_length++] = data[index];
        if (parser->buffered_length != FRAME_CODEC_SENSOR_FRAME_SIZE) {
            continue;
        }

        frame_codec_sensor_sample_t sample = {0};
        const frame_codec_result_t decode_result = frame_codec_decode_sensor_sample(
            parser->buffered_frame, parser->buffered_length, &sample);

        if (decode_result == FRAME_CODEC_OK) {
            ++parser->valid_frame_count;
            parser->buffered_length = 0U;
            callback(&sample, callback_context);
            continue;
        }

        if (decode_result == FRAME_CODEC_ERR_CRC_MISMATCH) {
            ++parser->crc_error_count;
        } else {
            ++parser->invalid_frame_count;
        }
        lora_stream_parser_resynchronize(parser);
    }
}

uint32_t lora_stream_parser_valid_frame_count(const lora_stream_parser_t *parser)
{
    return parser == NULL ? 0U : parser->valid_frame_count;
}

uint32_t lora_stream_parser_invalid_frame_count(const lora_stream_parser_t *parser)
{
    return parser == NULL ? 0U : parser->invalid_frame_count;
}

uint32_t lora_stream_parser_crc_error_count(const lora_stream_parser_t *parser)
{
    return parser == NULL ? 0U : parser->crc_error_count;
}

uint32_t lora_stream_parser_discarded_byte_count(const lora_stream_parser_t *parser)
{
    return parser == NULL ? 0U : parser->discarded_byte_count;
}
