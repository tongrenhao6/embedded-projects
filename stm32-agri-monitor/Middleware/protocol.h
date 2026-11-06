/* protocol.h - unified frame: HEAD|CMD|SRC|DST|LEN(2)|SEQ|DATA|CRC16|TAIL */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define PROTO_HEAD0            0xAAU
#define PROTO_HEAD1            0x55U
#define PROTO_TAIL0            0x0DU
#define PROTO_TAIL1            0x0AU
#define PROTO_MAX_PAYLOAD      128U
#define PROTO_MAX_FRAME        (2U + 1U + 1U + 1U + 2U + 1U + PROTO_MAX_PAYLOAD + 2U + 2U)

/* 命令码 */
#define PROTO_CMD_HEARTBEAT    0x01U
#define PROTO_CMD_SENSOR_UP    0x02U
#define PROTO_CMD_POLL_REQ     0x03U
#define PROTO_CMD_POLL_ACK     0x04U
#define PROTO_CMD_ALARM        0x05U
#define PROTO_CMD_CFG_REQ      0x06U
#define PROTO_CMD_CFG_ACK      0x07U
#define PROTO_CMD_BOOT         0x08U   /* OTA 预留 */
#define PROTO_CMD_CTRL         0x09U   /* 控制指令：payload[0]=0 静音 / 1 解除 */

/* 传感器数据负载（SENSOR_UP / POLL_ACK 共用）
* 固定 10 字节：t(2,0.1C) h(2,0.1%RH) co2(2) lux(2) soil:0..100 湿度百分比 alarm(1) */
typedef struct {
  uint16_t temp_x10;    /* -300..+800，x10 */
  uint16_t humi_x10;    /* 0..1000 */
  uint16_t co2_ppm;
  uint16_t light_lux;
  uint8_t  soil_grade;  /* 0..100 湿度百分比 */
  uint8_t  alarm_flags; /* bit0=temp bit1=humi bit2=co2 bit3=light bit4=soil */
} proto_sensor_payload_t;

/* 解析后的一帧信息 */
typedef struct {
  uint8_t  cmd;
  uint8_t  src;
  uint8_t  dst;
  uint8_t  seq;
  uint16_t len;
  uint8_t  payload[PROTO_MAX_PAYLOAD];
} proto_frame_t;

uint16_t protocol_crc16(const uint8_t *data, uint16_t len);
uint16_t protocol_build_frame(uint8_t cmd, uint8_t src, uint8_t dst, uint8_t seq,
                              const uint8_t *payload, uint16_t len,
                              uint8_t *out, uint16_t out_size);
int       protocol_check_frame(const uint8_t *buf, uint16_t len, proto_frame_t *frame);
uint16_t protocol_pack_sensor(const proto_sensor_payload_t *s, uint8_t *out);
int      protocol_unpack_sensor(const uint8_t *pl, uint16_t len, proto_sensor_payload_t *s);

#endif
