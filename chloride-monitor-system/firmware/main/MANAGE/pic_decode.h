#ifndef __PIC_DECODE_H
#define __PIC_DECODE_H

#include <stdint.h>

#define rgb565(r, g, b) ((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

typedef void (*lcd_write_cb)(uint32_t w, uint32_t h, uint8_t *pic_buf);

#endif
