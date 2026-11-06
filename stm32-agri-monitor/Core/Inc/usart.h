/* usart.h - USART1(debug) / USART2(datas: CO2 or ESP8266) */
#ifndef __USART_H
#define __USART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;

/* 就在这里加两行 */
//#define USART1_BAUDRATE   115200
//#define USART2_BAUDRATE   9600

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_UART2_Receive_Start(void);
void MX_UART1_Receive_Start(void);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H */
