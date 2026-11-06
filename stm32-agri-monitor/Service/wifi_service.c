/* wifi_service.c - ESP8266 over USART2 in NORMAL AT data mode (no transparent):
 *   connect TCP OneNET:1883, send via AT+CIPSEND=<len>, receive via +IPD,
 *   MQTT CONNECT(3-tuple) / PUBLISH OneJSON, parse CONNACK / reply PUBLISH.
 * (Port of 11_HALWIFI_Simple AT flow; route A: +IPD normal mode) */
#include "wifi_service.h"
#include "bsp_uart.h"
#include "param.h"
#include "log_service.h"
#include "main.h"
#include "app_config.h"
#include "cmsis_os2.h"
#include <string.h>
#include <stdio.h>

#define WIFI_RX_BUF_SIZE  512U

static uint8_t  g_wifi_rx_buf[WIFI_RX_BUF_SIZE];
static uint16_t g_wifi_rx_cnt;

static int g_connack_rc = -1;              /* 最近一次 CONNACK 返回码，-1=未知 */
static int g_suback_rc   = -1;              /* 最近一次 SUBACK 判定，-1=未知 */
static volatile uint8_t g_pingresp_received;
static uint8_t g_set_reply_pending;
static char g_set_reply_json[128];
static void (*s_ctrl_cb)(uint8_t beep_on) = 0;  /* 属性设置 beep_ctrl 回调 */

static int ipd_process(void);
static void wifi_rx_clear(void);

static uint32_t wifi_find_bytes(const uint8_t *needle, uint32_t needle_len)
{
  uint32_t i;

  if ((needle_len == 0U) || (needle_len > g_wifi_rx_cnt)) return 0xFFFFFFFFU;
  for (i = 0U; i <= (uint32_t)g_wifi_rx_cnt - needle_len; i++)
  {
    if (memcmp(&g_wifi_rx_buf[i], needle, needle_len) == 0) return i;
  }
  return 0xFFFFFFFFU;
}

static void wifi_rx_consume(uint32_t len)
{
  if (len >= g_wifi_rx_cnt)
  {
    wifi_rx_clear();
    return;
  }
  memmove(g_wifi_rx_buf, &g_wifi_rx_buf[len], g_wifi_rx_cnt - len);
  g_wifi_rx_cnt = (uint16_t)(g_wifi_rx_cnt - len);
  g_wifi_rx_buf[g_wifi_rx_cnt] = 0U;
}

static void wifi_rx_pull(void)
{
  uint8_t  tmp[64];
  uint16_t n = bsp_dat_used();
  uint16_t free_len;
  free_len = (uint16_t)(WIFI_RX_BUF_SIZE - 1U - g_wifi_rx_cnt);
  if (free_len == 0U) return;
  if (n > sizeof(tmp)) n = sizeof(tmp);
  if (n > free_len) n = free_len;
  if (n != 0U)
  {
    (void)bsp_dat_read(tmp, n);
    memcpy(&g_wifi_rx_buf[g_wifi_rx_cnt], tmp, n);
    g_wifi_rx_cnt = (uint16_t)(g_wifi_rx_cnt + n);
    g_wifi_rx_buf[g_wifi_rx_cnt] = 0U;
  }
}

static void wifi_rx_clear(void)
{
  g_wifi_rx_cnt = 0U;
  g_wifi_rx_buf[0] = 0U;
}

static int mqtt_extract_id(const char *text, char *id, uint32_t id_size)
{
  const char *p;
  const char *end;
  uint32_t n = 0U;

  if ((text == 0) || (id == 0) || (id_size < 2U)) return -1;
  p = strstr(text, "\"id\"");
  if (p == 0) return -1;
  p = strchr(p, ':');
  if (p == 0) return -1;
  p++;
  while ((*p == ' ') || (*p == '\t')) p++;

  if (*p == '"')
  {
    p++;
    end = strchr(p, '"');
  }
  else
  {
    end = p;
    while ((*end != 0) && (*end != ',') && (*end != '}')) end++;
  }
  if ((end == 0) || (end <= p)) return -1;

  while ((p < end) && (n + 1U < id_size)) id[n++] = *p++;
  id[n] = 0;
  return (n != 0U) ? 0 : -1;
}

static void mqtt_prepare_set_reply(const char *text, uint16_t code, const char *msg)
{
  char id[48];

  if (mqtt_extract_id(text, id, sizeof(id)) != 0)
    strcpy(id, "0");
  snprintf(g_set_reply_json, sizeof(g_set_reply_json),
           "{\"id\":\"%s\",\"code\":%u,\"msg\":\"%s\"}",
           id, (unsigned)code, msg);
  g_set_reply_pending = 1U;
}

static int wifi_find(const char *key, uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();
  uint32_t key_len = (uint32_t)strlen(key);
  for (;;)
  {
    wifi_rx_pull();
    if (wifi_find_bytes((const uint8_t *)key, key_len) != 0xFFFFFFFFU) return 0;

    /* 先保留并检查 AT 响应；否则 +IPD 在 SEND OK 前到达时，
       ipd_process() 会把前面的 SEND OK 一起消费掉。 */
    (void)ipd_process();
    if (wifi_find_bytes((const uint8_t *)key, key_len) != 0xFFFFFFFFU) return 0;
    if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms) return -1;
    osDelay(1U);
  }
}

static void wifi_send_at(const char *cmd)
{
  wifi_rx_clear();
  (void)bsp_dat_send((const uint8_t *)cmd, (uint16_t)strlen(cmd));
}

static void wifi_send_bytes(const uint8_t *data, uint32_t len)
{
  (void)bsp_dat_send(data, (uint16_t)len);
}

/* 普通模式定长发送：AT+CIPSEND=<len> -> '>' -> data -> SEND OK */
static int wifi_send_len(const uint8_t *data, uint32_t len)
{
  char cmd[32];

  snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%lu\r\n", (unsigned long)len);

  /* 发送失败后不重复发送 CIPSEND，避免 ESP8266 已进入等待数据状态时被再次污染。 */
  wifi_send_at(cmd);
  if (wifi_find(">", 2000U) != 0)
  {
    log_msg("wifi", "CIPSEND no prompt");
    return -1;
  }
  wifi_send_bytes(data, len);
  if (wifi_find("SEND OK", 3000U) == 0) return 0;
  log_msg("wifi", "CIPSEND no send ok");
  return -1;
}

void wifi_service_init(void)
{
  bsp_dat_flush();
  wifi_rx_clear();
  g_pingresp_received = 0U;
  g_set_reply_pending = 0U;
}

/* ================= MQTT 报文打包 ================= */

static uint32_t mqtt_put_remaining(uint8_t *out, uint32_t n, uint32_t len)
{
  uint32_t rl = len;
  do
  {
    uint8_t e = (uint8_t)(rl % 128U);
    rl /= 128U;
    if (rl > 0U) e |= 0x80U;
    out[n++] = e;
  } while (rl > 0U);
  return n;
}

static uint32_t mqtt_pack_connect(uint8_t *out)
{
  uint32_t n = 0U;
  uint32_t cl = (uint32_t)strlen(ONENET_CLIENTID);
  uint32_t ul = (uint32_t)strlen(ONENET_USERNAME);
  uint32_t pl = (uint32_t)strlen(ONENET_PASSWD);
  uint32_t data_len = 10U + (cl + 2U) + (ul + 2U) + (pl + 2U);

  out[n++] = 0x10U;
  n = mqtt_put_remaining(out, n, data_len);

  out[n++] = 0x00U; out[n++] = 0x04U;
  out[n++] = 'M'; out[n++] = 'Q'; out[n++] = 'T'; out[n++] = 'T';
  out[n++] = 0x04U;
  out[n++] = 0xC2U;
  out[n++] = 0x00U; out[n++] = 60U;

  out[n++] = (uint8_t)(cl >> 8); out[n++] = (uint8_t)(cl & 0xFFU);
  memcpy(&out[n], ONENET_CLIENTID, cl); n += cl;

  out[n++] = (uint8_t)(ul >> 8); out[n++] = (uint8_t)(ul & 0xFFU);
  memcpy(&out[n], ONENET_USERNAME, ul); n += ul;

  out[n++] = (uint8_t)(pl >> 8); out[n++] = (uint8_t)(pl & 0xFFU);
  memcpy(&out[n], ONENET_PASSWD, pl); n += pl;

  return n;
}

static uint32_t mqtt_pack_publish(uint8_t *out, const char *topic, const char *payload)
{
  uint32_t tl = (uint32_t)strlen(topic);
  uint32_t pl = (uint32_t)strlen(payload);
  uint32_t len = (tl + 2U) + pl;
  uint32_t n = 0U;

  out[n++] = 0x30U;
  n = mqtt_put_remaining(out, n, len);

  out[n++] = (uint8_t)(tl >> 8);
  out[n++] = (uint8_t)(tl & 0xFFU);
  memcpy(&out[n], topic, tl); n += tl;
  memcpy(&out[n], payload, pl); n += pl;
  return n;
}

/* SUBSCRIBE（QoS0）：0x82 remaining packetId 主题长 主题 QoS */
static uint32_t mqtt_pack_subscribe(uint8_t *out, const char *topic, uint16_t packet_id)
{
  uint32_t tl = (uint32_t)strlen(topic);
  uint32_t len = 2U + (tl + 2U) + 1U;
  uint32_t n = 0U;

  out[n++] = 0x82U;
  n = mqtt_put_remaining(out, n, len);

  out[n++] = (uint8_t)(packet_id >> 8);
  out[n++] = (uint8_t)(packet_id & 0xFFU);
  out[n++] = (uint8_t)(tl >> 8);
  out[n++] = (uint8_t)(tl & 0xFFU);
  memcpy(&out[n], topic, tl); n += tl;
  out[n++] = 0x00U;                  /* QoS0 */
  return n;
}

/* ================= MQTT 接收解析（作用于一段数据） ================= */

static void mqtt_handle_data(const uint8_t *d, uint32_t n)
{
  uint32_t i;

  /* PINGRESP：0xD0 0x00，证明 MQTT Broker 仍然可达。 */
  for (i = 0U; (i + 1U) < n; i++)
  {
    if ((d[i] == 0xD0U) && (d[i + 1U] == 0x00U))
    {
      g_pingresp_received = 1U;
      return;
    }
  }

  /* CONNACK：0x20 0x02 [ack flags] [rc] */
  for (i = 0U; (i + 3U) < n; i++)
  {
    if ((d[i] == 0x20U) && (d[i + 1] == 0x02U))
    {
      g_connack_rc = d[i + 3U];
      return;
    }
  }

  /* SUBACK：0x90 ... */
  for (i = 0U; i < n; i++)
  {
    if (d[i] == 0x90U)
    {
      g_suback_rc = 0;
      return;
    }
  }

  /* PUBLISH：0x30 剩余长 主题长 主题 payload */
  for (i = 0U; i < n; i++)
  {
    if (d[i] == 0x30U)
    {
      uint32_t idx = i + 1U;
      uint32_t rl = 0U, mul = 1U;
      uint32_t tlen, plen, tc;
      char topic[64];
      char text[160];

      while ((idx < n) && ((d[idx] & 0x80U) != 0U))
      {
        rl += ((uint32_t)d[idx] & 0x7FU) * mul; mul *= 128U; idx++;
      }
      if (idx >= n) return;
      rl += (uint32_t)d[idx] * mul; idx++;
      if ((idx + 2U) > n) return;

      tlen = (((uint32_t)d[idx] << 8) | d[idx + 1U]); idx += 2U;
      if (idx + tlen > n) return;

      /* 提取主题副本 */
      tc = (tlen < (uint32_t)(sizeof(topic) - 1U)) ? tlen : (uint32_t)(sizeof(topic) - 1U);
      memcpy(topic, &d[idx], tc);
      topic[tc] = '\0';
      idx += tlen;

      plen = n - idx;
      if (plen > (uint32_t)(sizeof(text) - 1U)) plen = sizeof(text) - 1U;
      memcpy(text, &d[idx], plen);
      text[plen] = '\0';

      /* 属性设置下行：topic==property/set，解析 beep_ctrl */
      if (strcmp(topic, ONENET_TOPIC_SET) == 0)
      {
        char *p = strstr(text, "beep_ctrl");
        uint8_t valid = 0U;
        if (p != 0)
        {
          char *v = strchr(p, ':');
          if (v != 0)
          {
            v++;
            while ((*v == ' ') || (*v == '\t')) v++;
            if ((strncmp(v, "true", 4U) == 0) && (s_ctrl_cb != 0))
            {
              s_ctrl_cb(1U);
            }
            else if ((strncmp(v, "false", 5U) == 0) && (s_ctrl_cb != 0))
            {
              s_ctrl_cb(0U);
            }
            if ((strncmp(v, "true", 4U) == 0) ||
                (strncmp(v, "false", 5U) == 0))
              valid = 1U;
          }
        }
        log_msg("wifi", "mqtt set rx: %s", text);
        if (valid != 0U)
          mqtt_prepare_set_reply(text, 200U, "success");
        else
          mqtt_prepare_set_reply(text, 400U, "invalid params");
        g_connack_rc = -1;
        return;
      }

      /* 其他 PUBLISH（post/reply 回执等）：code 判定 */
      if ((strstr(text, "\"code\":200") != 0) || (strstr(text, "code\":200\"") != 0))
        log_msg("wifi", "onenet accepted (code=200)");
      else
        log_msg("wifi", "mqtt reply payload=%s", text);

      g_connack_rc = -1;                  /* 非 CONNACK，保持未知 */
      return;
    }
  }
}

/* 从接收缓冲找并处理一个完整 +IPD 段；处理成功返回 1 */
static int ipd_process(void)
{
  static const uint8_t ipd_tag[] = "+IPD,";
  uint32_t off;
  uint32_t len = 0U;
  uint32_t idx;
  uint32_t i;

  off = wifi_find_bytes(ipd_tag, sizeof(ipd_tag) - 1U);
  if (off == 0xFFFFFFFFU) return 0;

  /* 丢弃 +IPD 前的 AT 文本，但保留 +IPD 本身等待完整数据。 */
  if (off != 0U) wifi_rx_consume(off);
  idx = 5U;                                      /* 长度数字起点 */

  while ((idx < g_wifi_rx_cnt) &&
         (g_wifi_rx_buf[idx] >= '0') && (g_wifi_rx_buf[idx] <= '9'))
  {
    len = len * 10U + (uint32_t)(g_wifi_rx_buf[idx] - '0');
    idx++;
  }
  if (idx >= g_wifi_rx_cnt) return 0;            /* 长度字段尚未收全 */
  if (g_wifi_rx_buf[idx] != ':') return 0;   /* 格式异常，忽略 */
  idx++;                                     /* data 起点 */

  if ((uint32_t)g_wifi_rx_cnt < (idx + len)) return 0;   /* 段未收全 */

  mqtt_handle_data(&g_wifi_rx_buf[idx], len);
  wifi_rx_consume(idx + len);
  for (i = 0U; (i < 2U) && (g_wifi_rx_cnt != 0U) &&
       (g_wifi_rx_buf[0] == ((i == 0U) ? '\r' : '\n')); i++)
    wifi_rx_consume(1U);
  return 1;
}

/* ================= 对外接口 ================= */

void wifi_service_mqtt_poll_rx(void)
{
  uint8_t pass;

  /* 先持续排空 USART2，再按完整 +IPD 段逐段处理。 */
  for (pass = 0U; pass < 16U; pass++)
  {
    wifi_rx_pull();
    if (bsp_dat_used() == 0U) break;
  }
  for (pass = 0U; pass < 16U; pass++)
  {
    if (ipd_process() == 0) break;
  }
}

int wifi_service_mqtt_flush_set_reply(void)
{
  if (g_set_reply_pending == 0U) return 0;
  if ((g_wifi_rx_cnt != 0U) || (bsp_dat_used() != 0U)) return 0;
  if (wifi_service_mqtt_publish(ONENET_TOPIC_SET_REPLY, g_set_reply_json) != 0)
  {
    log_msg("wifi", "mqtt set reply failed");
    return -1;
  }
  g_set_reply_pending = 0U;
  log_msg("wifi", "mqtt set reply sent");
  return 0;
}

/* 等待 CONNACK（通过 +IPD 段） */
static int mqtt_wait_connack(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  g_connack_rc = -1;
  for (;;)
  {
    wifi_rx_pull();
    if (g_wifi_rx_cnt > 0U) (void)ipd_process();
    if (g_connack_rc >= 0)
    {
      log_msg("wifi", "mqtt connack rc=%d", g_connack_rc);
      if (g_connack_rc == 0) return 0;
      return -1;
    }
    if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
    {
      log_msg("wifi", "mqtt connack timeout");
      return -2;
    }
    osDelay(5U);
  }
}

/* 等待 SUBACK（订阅确认，通过 +IPD 段） */
static int mqtt_wait_suback(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  g_suback_rc = -1;
  for (;;)
  {
    wifi_rx_pull();
    if (g_wifi_rx_cnt > 0U) (void)ipd_process();
    if (g_suback_rc >= 0)
    {
      log_msg("wifi", "mqtt suback ok");
      return 0;
    }
    if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
    {
      log_msg("wifi", "mqtt suback timeout");
      return -1;
    }
    osDelay(5U);
  }
}

/* 等待 MQTT PINGRESP；仅 SEND OK 不足以证明 TCP/MQTT 仍然在线。 */
static int mqtt_wait_pingresp(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  while (g_pingresp_received == 0U)
  {
    wifi_rx_pull();
    while ((g_wifi_rx_cnt > 0U) && (g_pingresp_received == 0U))
    {
      if (ipd_process() == 0) break;
    }
    if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms) return -1;
    osDelay(5U);
  }
  return 0;
}

/* CIPCLOSE 在不同 ESP8266 固件/连接状态下可能返回 CLOSED 或 ERROR。 */
static void wifi_wait_close(uint32_t timeout_ms)
{
  uint32_t start = HAL_GetTick();

  for (;;)
  {
    wifi_rx_pull();
    if ((wifi_find_bytes((const uint8_t *)"CLOSED", 6U) != 0xFFFFFFFFU) ||
        (wifi_find_bytes((const uint8_t *)"ERROR", 5U) != 0xFFFFFFFFU) ||
        (wifi_find_bytes((const uint8_t *)"link is not valid", 17U) != 0xFFFFFFFFU))
      return;
    if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms) return;
    osDelay(5U);
  }
}

static int wifi_send_ok(const char *cmd, uint32_t timeout_ms)
{
  wifi_send_at(cmd);
  return wifi_find("OK", timeout_ms);
}

int wifi_service_connect(void)
{
  char buf[64];
  uint8_t cbuf[320];
  uint32_t cn;

  /* 重连前清掉 USART2 和解析器残留，避免旧响应污染本次流程。 */
  bsp_dat_flush();
  wifi_rx_clear();

  wifi_send_at("AT\r\n");
  if (wifi_find("OK", 2000U) != 0)
  {
    log_msg("wifi", "AT no response");
    return -1;
  }

  if (wifi_send_ok("AT+CIPMODE=0\r\n", 2000U) != 0)
  {
    log_msg("wifi", "CIPMODE=0 failed");
    return -1;
  }
  if (wifi_send_ok("AT+CIPMUX=0\r\n", 2000U) != 0)
  {
    log_msg("wifi", "CIPMUX=0 failed");
    return -1;
  }

  wifi_send_at("AT+CWMODE_CUR=1\r\n");
  (void)wifi_find("OK", 2000U);

  snprintf(buf, sizeof(buf), "AT+CWJAP_CUR=\"%s\",\"%s\"\r\n",
           (char *)g_param.wifi_ssid, (char *)g_param.wifi_pswd);
  wifi_send_at(buf);
  if (wifi_find("WIFI GOT IP", 10000U) != 0)
    if (wifi_find("OK", 10000U) != 0)
    {
      log_msg("wifi", "AP join failed");
      return -1;
    }

  /* 清理旧 TCP 连接；无旧连接时 ERROR/link is not valid 也算完成。 */
  bsp_dat_flush();
  wifi_rx_clear();
  wifi_send_at("AT+CIPCLOSE\r\n");
  wifi_wait_close(2000U);
  bsp_dat_flush();
  wifi_rx_clear();

  snprintf(buf, sizeof(buf), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n",
           ONENET_IP, (unsigned)ONENET_PORT);
  wifi_send_at(buf);
  if (wifi_find("CONNECT", 15000U) != 0U)
  {
    log_msg("wifi", "TCP connect failed (no CONNECT)");
    return -2;
  }
  HAL_Delay(300U);                 /* 连接就绪窗口 */
  log_msg("wifi", "tcp connected");

  /* 发 MQTT CONNECT（普通模式定长发送），等 CONNACK */
  cn = mqtt_pack_connect(cbuf);
  if (wifi_send_len(cbuf, cn) != 0) return -3;

  if (mqtt_wait_connack(5000U) != 0)
  {
    log_msg("wifi", "mqtt connect failed");
    return -4;
  }
  log_msg("wifi", "mqtt connected");

  /* 订阅：上报回执 + 属性设置 */
  {
    uint8_t sbuf[160];
    uint32_t sn;
    uint16_t pid = 1U;

    sn = mqtt_pack_subscribe(sbuf, ONENET_TOPIC_REPLY, pid++);
    if (wifi_send_len(sbuf, sn) != 0) return -5;
    if (mqtt_wait_suback(3000U) != 0) return -6;

    sn = mqtt_pack_subscribe(sbuf, ONENET_TOPIC_SET, pid++);
    if (wifi_send_len(sbuf, sn) != 0) return -7;
    if (mqtt_wait_suback(3000U) != 0) return -8;

    log_msg("wifi", "mqtt subscribed");
  }
  return 0;
}

void wifi_service_close_tcp(void)
{
  bsp_dat_flush();
  wifi_rx_clear();
  wifi_send_at("AT+CIPCLOSE\r\n");
  wifi_wait_close(2000U);
  bsp_dat_flush();
  wifi_rx_clear();
}

/* PINGREQ 保活：0xC0 0x00（QoS0 空），普通模式定长发送 */
int wifi_service_mqtt_keepalive(void)
{
  uint8_t pbuf[4];
  uint32_t n = 0U;

  g_pingresp_received = 0U;
  pbuf[n++] = 0xC0U;   /* PINGREQ */
  pbuf[n++] = 0x00U;
  if (wifi_send_len(pbuf, n) != 0) return -1;
  if (mqtt_wait_pingresp(3000U) != 0)
  {
    log_msg("wifi", "pingresp timeout");
    return -1;
  }
  return 0;
}

int wifi_service_reset_module(void)
{
  bsp_dat_flush();
  wifi_rx_clear();
  wifi_send_at("AT+RST\r\n");
  if (wifi_find("ready", 10000U) != 0)
  {
    log_msg("wifi", "module reset timeout");
    return -1;
  }
  bsp_dat_flush();
  wifi_rx_clear();
  HAL_Delay(1000U);
  log_msg("wifi", "module reset ok");
  return 0;
}

int wifi_service_enter_transparent(void)
{
  /* 普通 AT 数据模式，无需进入透传；保留接口（空操作） */
  return 0;
}

int wifi_service_is_ready(void)
{
  return (g_wifi_rx_cnt > 0U) ? 1 : 0;
}

void wifi_service_set_ctrl_cb(void (*cb)(uint8_t beep_on))
{
  s_ctrl_cb = cb;
}

int wifi_service_send(const char *json)
{
  if (json == 0) return -1;
  return wifi_send_len((const uint8_t *)json, (uint32_t)strlen(json));
}

int wifi_service_mqtt_publish(const char *topic, const char *payload)
{
  uint8_t pbuf[512];
  uint32_t n;

  if ((topic == 0) || (payload == 0)) return -1;
  n = mqtt_pack_publish(pbuf, topic, payload);
  return wifi_send_len(pbuf, n);
}
