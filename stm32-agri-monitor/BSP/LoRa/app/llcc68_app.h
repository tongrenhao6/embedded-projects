#ifndef LLCC68_APP_H
#define LLCC68_APP_H

#include <stdint.h>

/** 接收完成回调类型。data 指向的缓冲区仅在回调执行期间有效。 */
typedef void (*llcc68_app_rx_callback_t)(const uint8_t* data, uint8_t length);

/** 初始化 LLCC68 及 LoRa 调制、数据包和中断参数。 */
void llcc68_app_init(void);
/** 让 LLCC68 进入连续接收模式。 */
void llcc68_app_start_receive(void);
/** 将指定载荷写入芯片缓冲区并启动发送。 */
void llcc68_app_send(const uint8_t* data, uint8_t length);
/** 在主循环中读取并处理 DIO1 对应的射频中断。 */
void llcc68_app_process_irq(void);
/** 注册收到有效载荷后调用的上层处理函数。 */
void llcc68_app_set_rx_callback(llcc68_app_rx_callback_t callback);

#endif
