#include "frame_codec.h"

#define FRAME_CODEC_MAGIC 0xB1U
#define FRAME_CODEC_MESSAGE_TYPE_SENSOR_SAMPLE 0x01U
#define FRAME_CODEC_PAYLOAD_SIZE 20U
#define FRAME_CODEC_CRC_OFFSET (FRAME_CODEC_SENSOR_FRAME_SIZE - 2U)

static void write_u16_be(uint8_t *buffer, uint16_t value)
{
    buffer[0] = (uint8_t)(value >> 8U);
    buffer[1] = (uint8_t)value;
}

static void write_u32_be(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)(value >> 24U);
    buffer[1] = (uint8_t)(value >> 16U);
    buffer[2] = (uint8_t)(value >> 8U);
    buffer[3] = (uint8_t)value;
}

static uint16_t read_u16_be(const uint8_t *buffer)
{
    return (uint16_t)(((uint16_t)buffer[0] << 8U) | buffer[1]);
}

static uint32_t read_u32_be(const uint8_t *buffer)
{
    return ((uint32_t)buffer[0] << 24U) | ((uint32_t)buffer[1] << 16U) |
           ((uint32_t)buffer[2] << 8U) | buffer[3];
}

static uint16_t crc16_ccitt_false(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFU;

    for (size_t index = 0; index < length; ++index) {
        crc ^= (uint16_t)data[index] << 8U;
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1U) ^ 0x1021U)
                                        : (uint16_t)(crc << 1U);
        }
    }

    return crc;
}

frame_codec_result_t frame_codec_encode_sensor_sample(
    const frame_codec_sensor_sample_t *sample,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *encoded_length)
{
    if (encoded_length != NULL) {
        *encoded_length = 0U;
    }

    if (sample == NULL || frame == NULL) {
        return FRAME_CODEC_ERR_INVALID_ARG;
    }
    if (frame_capacity < FRAME_CODEC_SENSOR_FRAME_SIZE) {
        return FRAME_CODEC_ERR_BUFFER_TOO_SMALL;
    }

    frame[0] = FRAME_CODEC_MAGIC;
    frame[1] = FRAME_CODEC_PROTOCOL_VERSION;
    frame[2] = FRAME_CODEC_MESSAGE_TYPE_SENSOR_SAMPLE;
    frame[3] = FRAME_CODEC_PAYLOAD_SIZE;
    write_u16_be(&frame[4], sample->node_id);
    write_u32_be(&frame[6], sample->sequence);
    write_u32_be(&frame[10], sample->uptime_ms);
    write_u16_be(&frame[14], (uint16_t)sample->inside_temperature_centi_c);
    write_u16_be(&frame[16], sample->inside_humidity_centi_pct);
    write_u16_be(&frame[18], (uint16_t)sample->outside_temperature_centi_c);
    write_u16_be(&frame[20], sample->outside_humidity_centi_pct);
    write_u16_be(&frame[22], sample->status_flags);
    write_u16_be(&frame[FRAME_CODEC_CRC_OFFSET],
                 crc16_ccitt_false(frame, FRAME_CODEC_CRC_OFFSET));

    if (encoded_length != NULL) {
        *encoded_length = FRAME_CODEC_SENSOR_FRAME_SIZE;
    }

    return FRAME_CODEC_OK;
}

frame_codec_result_t frame_codec_decode_sensor_sample(
    const uint8_t *frame,
    size_t frame_length,
    frame_codec_sensor_sample_t *sample)
{
    if (frame == NULL || sample == NULL) {
        return FRAME_CODEC_ERR_INVALID_ARG;
    }
    if (frame_length != FRAME_CODEC_SENSOR_FRAME_SIZE ||
        frame[0] != FRAME_CODEC_MAGIC ||
        frame[1] != FRAME_CODEC_PROTOCOL_VERSION ||
        frame[2] != FRAME_CODEC_MESSAGE_TYPE_SENSOR_SAMPLE ||
        frame[3] != FRAME_CODEC_PAYLOAD_SIZE) {
        return FRAME_CODEC_ERR_INVALID_FRAME;
    }
    if (read_u16_be(&frame[FRAME_CODEC_CRC_OFFSET]) !=
        crc16_ccitt_false(frame, FRAME_CODEC_CRC_OFFSET)) {
        return FRAME_CODEC_ERR_CRC_MISMATCH;
    }

    sample->node_id = read_u16_be(&frame[4]);
    sample->sequence = read_u32_be(&frame[6]);
    sample->uptime_ms = read_u32_be(&frame[10]);
    sample->inside_temperature_centi_c = (int16_t)read_u16_be(&frame[14]);
    sample->inside_humidity_centi_pct = read_u16_be(&frame[16]);
    sample->outside_temperature_centi_c = (int16_t)read_u16_be(&frame[18]);
    sample->outside_humidity_centi_pct = read_u16_be(&frame[20]);
    sample->status_flags = read_u16_be(&frame[22]);

    return FRAME_CODEC_OK;
}
