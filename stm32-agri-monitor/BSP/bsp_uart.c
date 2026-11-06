/* bsp_uart.c - IT rx by role:
 *   USART1: debug -> ringbuf (both roles)
 *   USART2: terminal(CO2)  fixed 6-byte 0x2C frames, real-time callback
 *           gateway(ESP8266) single-byte -> ringbuf
 *   send via HAL blocking (TXE register write is done in main.c fputc) */
#include "bsp_uart.h"
#include "usart.h"
#include "main.h"
#include "app_config.h"
#include "sensor_service.h"

#include "log_service.h"

static uint8_t rb_dbg_mem[UART_DBG_BUF_SIZE];
static uint8_t rb_dat_mem[UART_DAT_BUF_SIZE];
ringbuf_t uart_dbg_rb;
ringbuf_t uart_dat_rb;

static uint8_t rx_dbg_byte;
static uint8_t rx_dat_byte;     /* 网关: 单字节 (ESP8266) */


void bsp_uart_init(void)
{
  rb_init(&uart_dbg_rb, rb_dbg_mem, UART_DBG_BUF_SIZE);
  rb_init(&uart_dat_rb, rb_dat_mem, UART_DAT_BUF_SIZE);

  (void)HAL_UART_Receive_IT(&huart1, &rx_dbg_byte, 1U);
#if (ROLE_GATEWAY == 1U)
  (void)HAL_UART_Receive_IT(&huart2, &rx_dat_byte, 1U);
#else
  (void)HAL_UART_Receive_IT(&huart2, &rx_dat_byte, 1U);
#endif
}

uint16_t bsp_dbg_used(void){ return rb_used(&uart_dbg_rb); }
uint16_t bsp_dat_used(void){ return rb_used(&uart_dat_rb); }
uint16_t bsp_dbg_read(uint8_t *d, uint16_t l){ return rb_read(&uart_dbg_rb, d, l); }
uint16_t bsp_dat_read(uint8_t *d, uint16_t l){ return rb_read(&uart_dat_rb, d, l); }

void bsp_dat_flush(void)
{
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  uart_dat_rb.tail = uart_dat_rb.head;
  if (primask == 0U) __enable_irq();
}

void bsp_dbg_send(const uint8_t *data, uint16_t len)
{
  (void)HAL_UART_Transmit(&huart1, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void bsp_dat_send(const uint8_t *data, uint16_t len)
{
  (void)HAL_UART_Transmit(&huart2, (uint8_t *)data, len, HAL_MAX_DELAY);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    (void)rb_write(&uart_dbg_rb, &rx_dbg_byte, 1U);
    (void)HAL_UART_Receive_IT(&huart1, &rx_dbg_byte, 1U);
  }
  else if (huart->Instance == USART2)
  {
#if (ROLE_GATEWAY == 1U)
    (void)rb_write(&uart_dat_rb, &rx_dat_byte, 1U);
    (void)HAL_UART_Receive_IT(&huart2, &rx_dat_byte, 1U);
#else
    sensor_service_co2_rx_byte(rx_dat_byte);
    (void)HAL_UART_Receive_IT(&huart2, &rx_dat_byte, 1U);
#endif
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    (void)HAL_UART_Receive_IT(&huart1, &rx_dbg_byte, 1U);
  }
  else if (huart->Instance == USART2)
  {
    (void)HAL_UART_Receive_IT(&huart2, &rx_dat_byte, 1U);
  }
}
