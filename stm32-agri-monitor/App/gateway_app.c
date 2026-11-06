/* gateway_app.c - gateway: LoRa poll nodes -> WIFI(TCP/JSON) upload -> OLED */
#include "gateway_app.h"
#include "main.h"
#include "app_config.h"
#include "bsp_uart.h"
#include "bsp_gpio.h"
#include "bsp_rtc.h"
#include "lora_service.h"
#include "wifi_service.h"
#include "oled_service.h"
#include "log_service.h"
#include "param.h"
#include "protocol.h"
#include "oled_service.h"
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

#define GW_POLL_WAIT_MS  800U    /* 应答等待窗口加大，减少超时丢轮 */
#define GW_POLL_MS      1000U    /* 网关轮询周期（独立，1s） */
#define GW_WIFI_RETRY_MS 15000U

/* OneNET 物模型要求 13 位毫秒时间戳；HAL_GetTick() 为运行毫秒，
   加固定起始时间戳（2023-11-14 前后）合成合法 13 位单调时间。 */
#define ONENET_TIME_BASE_MS  1789689600000ULL   /* 2026-09-18 00:00 UTC 毫秒，当前时间基座 */

static gw_slot_t g_slots[MAX_NODE_NUM];
static volatile uint8_t g_lora_ok;
static volatile uint8_t g_wifi_ok;
static volatile uint8_t g_wait_src;   /* node addr we are waiting ACK from */
static volatile uint8_t g_got_ack;

static void gw_rx_handler(const proto_frame_t *frame);
static void gw_ctrl_cb(uint8_t beep_on);   /* OneNET 属性设置 beep_ctrl 回调 */

void gateway_app_init(void)
{
  uint8_t i;
  for (i = 0U; i < MAX_NODE_NUM; i++)
  {
    g_slots[i].id    = (uint8_t)(NODE_ID_BASE + i);
    g_slots[i].valid = 0U;
  }
  g_lora_ok = 0U;
  g_wifi_ok = 0U;
  wifi_service_init();
  wifi_service_set_ctrl_cb(gw_ctrl_cb);
  oled_service_init();
  lora_service_init(gw_rx_handler);
  lora_service_start();
  log_msg("gw", "app init, nodes=%u", g_param.node_count);
}

/* incoming frame: node answers poll / pushes sensor data */
static void gw_rx_handler(const proto_frame_t *frame)
{
  if ((frame->cmd == PROTO_CMD_POLL_ACK) || (frame->cmd == PROTO_CMD_SENSOR_UP))
  {
    proto_sensor_payload_t sp;
    uint8_t idx;

    if (frame->src < NODE_ID_BASE) return;
    idx = (uint8_t)(frame->src - NODE_ID_BASE);
    if (idx >= MAX_NODE_NUM) return;
    if (protocol_unpack_sensor(frame->payload, frame->len, &sp) != 0) return;

    g_slots[idx].id       = frame->src;
    g_slots[idx].valid    = 1U;
    g_slots[idx].alarm    = sp.alarm_flags;
    g_slots[idx].temp_x10 = sp.temp_x10;
    g_slots[idx].humi_x10 = sp.humi_x10;
    g_slots[idx].co2_ppm  = sp.co2_ppm;
    g_slots[idx].light_lux= sp.light_lux;
    g_slots[idx].soil_grade= sp.soil_grade;

    if ((g_wait_src != 0U) && (frame->src == g_wait_src)) g_got_ack = 1U;
    log_msg("gw", "rx from 0x%02X co2=%u", frame->src, sp.co2_ppm);
  }
}

/* OneNET 属性设置 beep_ctrl：1=允许响(解除静音) 0=静音 → LoRa CTRL 下发终端 */
static void gw_ctrl_cb(uint8_t beep_on)
{
  uint8_t pay[1];

  pay[0] = (beep_on != 0U) ? 0x01U : 0x00U;
  lora_service_queue_frame(PROTO_CMD_CTRL, (uint8_t)(NODE_ID_BASE + 0U), pay, 1U);
  log_msg("gw", "ctrl beep=%u -> node", (unsigned)beep_on);
}

static void gw_poll_node(uint8_t node_addr)
{
  g_wait_src = node_addr;
  g_got_ack  = 0U;
  lora_service_queue_frame(PROTO_CMD_POLL_REQ, node_addr, 0, 0U);

  {
    uint32_t start = HAL_GetTick();
    while (g_got_ack == 0U)
    {
      lora_service_task();
      if ((uint32_t)(HAL_GetTick() - start) >= GW_POLL_WAIT_MS)
        break;
      osDelay(20U);
    }
  }
}

/* ---- LoRa polling task ---------------------------------------- */
void GatewayLoraTask(void *argument)
{
  uint8_t i;
  uint8_t count = g_param.node_count;
  if (count > MAX_NODE_NUM) count = MAX_NODE_NUM;

  for (;;)
  {
    for (i = 0U; i < count; i++)
    {
      gw_poll_node((uint8_t)(NODE_ID_BASE + i));
    }
    g_lora_ok = 1U;
    osDelay(GW_POLL_MS);
  }
}

/* ---- WIFI upload task ----------------------------------------- */
void GatewayWifiTask(void *argument)
{
  char onejson[512];
  uint32_t next;
  uint32_t last_ka;
  uint8_t reconnect_failures;

  next    = HAL_GetTick();
  last_ka = HAL_GetTick();
  reconnect_failures = 0U;
  for (;;)
  {
    wifi_service_mqtt_poll_rx();          /* 持续排空 USART2 并解析 OneNET 下行 */

    if (g_wifi_ok == 0U)
    {
      if (wifi_service_connect() == 0)
      {
        if (wifi_service_enter_transparent() == 0)
        {
          g_wifi_ok = 1U;
          reconnect_failures = 0U;
          next = HAL_GetTick();
          last_ka = HAL_GetTick();
        }
      }
      if (g_wifi_ok == 0U)
      {
        reconnect_failures++;
        log_msg("gw", "wifi reconnect failed x%u", (unsigned)reconnect_failures);
        if (reconnect_failures >= 3U)
        {
          log_msg("gw", "wifi reconnect exhausted, reset module");
          (void)wifi_service_reset_module();
          reconnect_failures = 0U;
        }
        osDelay(GW_WIFI_RETRY_MS);
        continue;
      }
    }

    if (wifi_service_mqtt_flush_set_reply() != 0)
    {
      g_wifi_ok = 0U;
      wifi_service_close_tcp();
      continue;
    }

    /* MQTT 心跳保活：每 40s 发 PINGREQ，失败即判定连接已死并重连 */
    if ((uint32_t)(HAL_GetTick() - last_ka) >= 40000U)
    {
      last_ka = HAL_GetTick();
      if (wifi_service_mqtt_keepalive() != 0)
      {
        log_msg("gw", "keepalive fail, reconnect");
        g_wifi_ok = 0U;
        wifi_service_close_tcp();
        continue;
      }
      else
      {
        log_msg("gw", "keepalive ok");
      }
    }

    if ((uint32_t)(HAL_GetTick() - next) >= g_param.upload_ms)
    {
      next = HAL_GetTick();
      /* OneNET 属性上报（OneJSON）：单设备＝本网关第一节点数据 */
      snprintf(onejson, sizeof(onejson),
        "{\"id\":\"123\",\"version\":\"1.0\",\"params\":{"
        "\"CurrentTemperature\":{\"value\":%.1f,\"time\":%llu},"
        "\"RelativeHumidity\":{\"value\":%.1f,\"time\":%llu},"
        "\"CO2Value\":{\"value\":%u,\"time\":%llu},"
        "\"LightLux\":{\"value\":%u,\"time\":%llu},"
        "\"SoilMoisture\":{\"value\":%u,\"time\":%llu}}}",
        (double)g_slots[0].temp_x10 / 10.0,
        (unsigned long long)(ONENET_TIME_BASE_MS + HAL_GetTick()),
        (double)g_slots[0].humi_x10 / 10.0,
        (unsigned long long)(ONENET_TIME_BASE_MS + HAL_GetTick()),
        (unsigned)g_slots[0].co2_ppm,
        (unsigned long long)(ONENET_TIME_BASE_MS + HAL_GetTick()),
        (unsigned)g_slots[0].light_lux,
        (unsigned long long)(ONENET_TIME_BASE_MS + HAL_GetTick()),
        (unsigned)g_slots[0].soil_grade,
        (unsigned long long)(ONENET_TIME_BASE_MS + HAL_GetTick()));

      if (wifi_service_mqtt_publish(ONENET_TOPIC_UP, onejson) != 0)
      {
        log_msg("gw", "publish fail, reconnect");   /* CIPSEND 已双重试仍失败→连接已死，立即重连 */
        g_wifi_ok = 0U;
        wifi_service_close_tcp();
      }
      else
      {
        log_msg("gw", "onenet publish len=%d", (int)strlen(onejson));
      }
    }
    osDelay(200U);
  }
}

/* ---- OLED display task ---------------------------------------- */
void GatewayDisplayTask(void *argument)
{
  for (;;)
  {
    oled_service_gw_screen(g_slots, g_param.node_count, g_lora_ok, g_wifi_ok,
                           bsp_rtc_timestamp());
    osDelay(1000U);
  }
}
