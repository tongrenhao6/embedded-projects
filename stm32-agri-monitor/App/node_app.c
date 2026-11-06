/* node_app.c - terminal: collect -> report via LoRa -> local alarm -> OLED */
#include "node_app.h"
#include "main.h"
#include "app_config.h"
#include "bsp_dht22.h"
#include "bsp_uart.h"
#include "bsp_gpio.h"
#include "bsp_rtc.h"
#include "sensor_service.h"
#include "lora_service.h"
#include "alarm_service.h"
#include "oled_service.h"
#include "storage_service.h"
#include "log_service.h"
#include "param.h"
#include "protocol.h"
#include "cmsis_os2.h"
#include <string.h>

//extern sensor_data_t g_sensor;

/* ---- local LoRa receive handler: answer poll, ignore cfg ---- */
static void node_rx_handler(const proto_frame_t *frame);

void node_app_init(void)
{
  bsp_dht22_init();
  sensor_service_init();
  alarm_service_init();
  oled_service_init();
  lora_service_init(node_rx_handler);
  lora_service_start();
  log_msg("node", "app init done, node_id=%u", g_param.node_id);
}

static void node_rx_handler(const proto_frame_t *frame)
{
  if (frame->dst != g_param.node_id) return;   /* not for us */

  if (frame->cmd == PROTO_CMD_POLL_REQ)
  {
    uint8_t pl[PROTO_MAX_PAYLOAD];
    proto_sensor_payload_t sp;

    sp.temp_x10  = (uint16_t)(g_sensor.temp * 10.0f);
    sp.humi_x10  = (uint16_t)(g_sensor.humi * 10.0f);
    sp.co2_ppm   = g_sensor.co2_ppm;
    sp.light_lux = g_sensor.light_lux;
    sp.soil_grade= g_sensor.soil_grade;
    sp.alarm_flags = g_sensor.alarm_flags;

    (void)protocol_pack_sensor(&sp, pl);
    lora_service_queue_frame(PROTO_CMD_POLL_ACK, frame->src, pl, 10U);
    log_msg("node", "answered poll from 0x%02X", frame->src);
  }
  else if ((frame->cmd == PROTO_CMD_CTRL) && (frame->len >= 1U))
  {
    /* OneNET false=静音，true=解除静音；实际报警输出暂用 LED1。 */
    alarm_service_mute((frame->payload[0] == 0x00U) ? 1U : 0U);
    log_msg("node", "ctrl beep=%u mute=%u", (unsigned)frame->payload[0],
            (unsigned)((frame->payload[0] == 0x00U) ? 1U : 0U));
  }
  else if (frame->cmd == PROTO_CMD_HEARTBEAT)
  {
    log_msg("node", "heartbeat from 0x%02X", frame->src);
  }
  else
  {
    log_msg("node", "recv cmd 0x%02X from 0x%02X", frame->cmd, frame->src);
  }
}

/* ---- sensor task: periodic collect + threshold ---------------- */
void NodeSensorTask(void *argument)
{
  uint32_t period = g_param.collect_ms;

  for (;;)
  {
    sensor_service_collect();

    g_sensor.alarm_flags = alarm_service_check((const sensor_data_t *)&g_sensor);
    alarm_service_beep(g_sensor.alarm_flags);

    osDelay(period);
  }
}

/* ---- lora task: pump IRQ + periodic SENSOR_UP report ---------- */
//只在收到网关 POLL_REQ 时由 node_rx_handler 应答 POLL_ACK（该逻辑已在，无需改）。主动 SENSOR_UP 移除后，双方同频不再互撞。
void NodeLoraTask(void *argument)
{
  for (;;)
  {
    lora_service_task();      /* 只处理 IRQ 和转发应答帧（由 rx_handler 排队） */
    osDelay(20U);
  }
}

/* ---- alarm task: short beeps while alarm active ---------------- */
void NodeAlarmTask(void *argument)
{
  uint8_t last = 0U;
  for (;;)
  {
    uint8_t f = g_sensor.alarm_flags;
    if (f != last)
    {
      last = f;
      alarm_service_beep(f);
      log_msg("alarm", "flags=0x%02X", f);
    }
    alarm_service_poll();          /* 静音超时后自动恢复允许响 */
    osDelay(300U);
  }
}

/* ---- display task ----------------------------------------------- */
void NodeDisplayTask(void *argument)
{
  for (;;)
  {
    oled_service_node_screen((const sensor_data_t *)&g_sensor,
                             1U, bsp_rtc_timestamp());
    osDelay(1000U);
  }
}
