/* lora_service.h - LLCC68 wrapper: protocol frames over LoRa, single-writer task */
#ifndef LORA_SERVICE_H
#define LORA_SERVICE_H

#include <stdint.h>
#include "protocol.h"

typedef void (*lora_rx_handler_t)(const proto_frame_t *frame);

void lora_service_init(lora_rx_handler_t handler);
void lora_service_start(void);   /* init chip + enter continuous RX */
void lora_service_queue_frame(uint8_t cmd, uint8_t dst,
                              const uint8_t *payload, uint16_t len);
void lora_service_task(void);    /* must be called from Lora task loop */

#endif
