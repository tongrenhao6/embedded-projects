/* lora_service.c - only the Lora task touches the radio (SPI blocking op) */
#include "lora_service.h"
#include "llcc68_app.h"
#include "bsp_gpio.h"
#include "param.h"
#include "log_service.h"
#include "main.h"
#include <string.h>

#define LORA_IRQ_POLL_MS  10U
#define LORA_TX_QUEUE_LEN  4U

static lora_rx_handler_t g_handler;

typedef struct {
  uint8_t  cmd;
  uint8_t  dst;
  uint16_t len;
  uint8_t  payload[PROTO_MAX_PAYLOAD];
} lora_tx_item_t;

static lora_tx_item_t g_ctrl_item;
static volatile uint8_t g_ctrl_pending;
static lora_tx_item_t g_tx_queue[LORA_TX_QUEUE_LEN];
static volatile uint8_t g_tx_head;
static volatile uint8_t g_tx_tail;
static uint8_t  g_seq;

static uint8_t lora_tx_next(lora_tx_item_t *item)
{
  uint32_t primask = __get_PRIMASK();

  __disable_irq();
  if (g_ctrl_pending != 0U)
  {
    *item = g_ctrl_item;
    g_ctrl_pending = 0U;
    if (primask == 0U) __enable_irq();
    return 1U;
  }
  if (g_tx_tail != g_tx_head)
  {
    *item = g_tx_queue[g_tx_tail];
    g_tx_tail = (uint8_t)((g_tx_tail + 1U) % LORA_TX_QUEUE_LEN);
    if (primask == 0U) __enable_irq();
    return 1U;
  }
  if (primask == 0U) __enable_irq();
  return 0U;
}

static void lora_rx_raw(const uint8_t *data, uint8_t length)
{
  proto_frame_t f;
  if (protocol_check_frame(data, length, &f) == 0)
  {
    if (g_handler != 0) g_handler(&f);
  }
  else
  {
    log_msg("lora", "bad frame len=%d", (int)length);
  }
}

void lora_service_init(lora_rx_handler_t handler)
{
  g_handler = handler;
  g_seq = 0U;
  g_ctrl_pending = 0U;
  g_tx_head = 0U;
  g_tx_tail = 0U;
  llcc68_app_set_rx_callback(lora_rx_raw);
}

void lora_service_start(void)
{
  llcc68_app_init();
  llcc68_app_start_receive();
  log_msg("lora", "LLCC68 started");
}

void lora_service_queue_frame(uint8_t cmd, uint8_t dst,
                              const uint8_t *payload, uint16_t len)
{
  uint8_t next;
  uint32_t primask;

  if (len > PROTO_MAX_PAYLOAD) len = PROTO_MAX_PAYLOAD;

  primask = __get_PRIMASK();
  __disable_irq();
  if (cmd == PROTO_CMD_CTRL)
  {
    g_ctrl_item.cmd = cmd;
    g_ctrl_item.dst = dst;
    g_ctrl_item.len = len;
    if ((payload != 0) && (len != 0U))
      memcpy(g_ctrl_item.payload, payload, len);
    g_ctrl_pending = 1U;
  }
  else
  {
    next = (uint8_t)((g_tx_head + 1U) % LORA_TX_QUEUE_LEN);
    if (next == g_tx_tail)
    {
      if (primask == 0U) __enable_irq();
      log_msg("lora", "TX queue full, drop cmd=0x%02X", cmd);
      return;
    }
    g_tx_queue[g_tx_head].cmd = cmd;
    g_tx_queue[g_tx_head].dst = dst;
    g_tx_queue[g_tx_head].len = len;
    if ((payload != 0) && (len != 0U))
      memcpy(g_tx_queue[g_tx_head].payload, payload, len);
    g_tx_head = next;
  }
  if (primask == 0U) __enable_irq();
}

void lora_service_task(void)
{
  static uint32_t last_poll_ms = 0U;
  uint32_t now = HAL_GetTick();

  {
    lora_tx_item_t item;
    if (lora_tx_next(&item) != 0U)
    {
    uint8_t frame[PROTO_MAX_FRAME];
    uint16_t n = protocol_build_frame(item.cmd, (uint8_t)g_param.node_id,
                                      item.dst, g_seq++,
                                      item.payload, item.len,
                                      frame, sizeof(frame));
    if (n > 0U)
    {
      llcc68_app_send(frame, (uint8_t)n);
      log_msg("lora", "TX cmd=0x%02X dst=0x%02X len=%d", item.cmd, item.dst, n);
    }
    }
  }

  if ((lora_dio1_pending != 0U) ||
      ((uint32_t)(now - last_poll_ms) >= LORA_IRQ_POLL_MS))
  {
    lora_dio1_pending = 0U;
    last_poll_ms = now;
    llcc68_app_process_irq();
  }
}
