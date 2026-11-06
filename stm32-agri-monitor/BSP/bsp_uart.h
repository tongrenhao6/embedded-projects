/* bsp_uart.h - UART layer: USART1(debug/log) + USART2(datas: CO2|ESP8266), IRQ rx + ringbuf */
#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>
#include "ringbuf.h"

#define UART_DBG_BUF_SIZE  128U
#define UART_DAT_BUF_SIZE  512U

extern ringbuf_t uart_dbg_rb;
extern ringbuf_t uart_dat_rb;

void     bsp_uart_init(void);
uint16_t bsp_dbg_used(void);
uint16_t bsp_dbg_read(uint8_t *data, uint16_t len);
uint16_t bsp_dat_used(void);
uint16_t bsp_dat_read(uint8_t *data, uint16_t len);
void     bsp_dat_flush(void);
void     bsp_dbg_send(const uint8_t *data, uint16_t len);
void     bsp_dat_send(const uint8_t *data, uint16_t len);

#endif
