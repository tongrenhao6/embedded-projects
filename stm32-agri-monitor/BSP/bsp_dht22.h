/* bsp_dht22.h - DHT22 single-bus via GPIO PC15 (DWT-based timing) */
#ifndef BSP_DHT22_H
#define BSP_DHT22_H

#include <stdint.h>

void     bsp_dht22_init(void);
int      bsp_dht22_read(float *humi, float *temp);  /* 0 = OK, <0 = error */

#endif
