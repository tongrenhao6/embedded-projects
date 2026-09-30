#pragma once

#include <stddef.h>
#include <stdint.h>

#include "frame_codec.h"

#define LORA_STREAM_PARSER_FRAME_MAGIC 0xB1U

typedef void (*lora_stream_parser_frame_callback_t)(
    const frame_codec_sensor_sample_t *sample,
    void *context);

typedef struct {
    uint8_t buffered_frame[FRAME_CODEC_SENSOR_FRAME_SIZE];
    size_t buffered_length;
    uint32_t valid_frame_count;
    uint32_t invalid_frame_count;
    uint32_t crc_error_count;
    uint32_t discarded_byte_count;
} lora_stream_parser_t;

void lora_stream_parser_init(lora_stream_parser_t *parser);

void lora_stream_parser_push(lora_stream_parser_t *parser,
                             const uint8_t *data,
                             size_t data_length,
                             lora_stream_parser_frame_callback_t callback,
                             void *callback_context);

uint32_t lora_stream_parser_valid_frame_count(const lora_stream_parser_t *parser);
uint32_t lora_stream_parser_invalid_frame_count(const lora_stream_parser_t *parser);
uint32_t lora_stream_parser_crc_error_count(const lora_stream_parser_t *parser);
uint32_t lora_stream_parser_discarded_byte_count(const lora_stream_parser_t *parser);
