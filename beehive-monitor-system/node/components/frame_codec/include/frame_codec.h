#pragma once

#include <stddef.h>
#include <stdint.h>

#define FRAME_CODEC_PROTOCOL_VERSION 1U
#define FRAME_CODEC_SENSOR_FRAME_SIZE 26U
#define FRAME_CODEC_STATUS_INSIDE_SENSOR_VALID (1U << 0U)
#define FRAME_CODEC_STATUS_OUTSIDE_SENSOR_VALID (1U << 1U)
#define FRAME_CODEC_STATUS_TILT_VALID (1U << 2U)
#define FRAME_CODEC_STATUS_TILTED (1U << 3U)

typedef enum {
    FRAME_CODEC_OK = 0,
    FRAME_CODEC_ERR_INVALID_ARG,
    FRAME_CODEC_ERR_BUFFER_TOO_SMALL,
    FRAME_CODEC_ERR_INVALID_FRAME,
    FRAME_CODEC_ERR_CRC_MISMATCH,
} frame_codec_result_t;

typedef struct {
    uint16_t node_id;                       // 节点ID
    uint32_t sequence;                      // 序列号  
    uint32_t uptime_ms;                     // 上次更新时间（毫秒）
    int16_t inside_temperature_centi_c;     // 室内温度（摄氏度的百分之一）
    uint16_t inside_humidity_centi_pct;     // 室内湿度（百分之一）
    int16_t outside_temperature_centi_c;    // 室外温度（摄氏度的百分之一）
    uint16_t outside_humidity_centi_pct;    // 室外湿度（百分之一）
    uint16_t status_flags;                  // 状态标志
} frame_codec_sensor_sample_t;

frame_codec_result_t frame_codec_encode_sensor_sample(
    const frame_codec_sensor_sample_t *sample,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *encoded_length);

frame_codec_result_t frame_codec_decode_sensor_sample(
    const uint8_t *frame,
    size_t frame_length,
    frame_codec_sensor_sample_t *sample);
