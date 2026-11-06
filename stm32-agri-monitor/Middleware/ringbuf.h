/* ringbuf.h - lightweight single-producer/single-consumer byte ring buffer */
#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>

#define RB_DEFAULT_SIZE 512U

typedef struct {
  uint8_t  *buf;
  uint16_t  size;
  volatile uint16_t head;  /* write index */
  volatile uint16_t tail;  /* read index */
} ringbuf_t;

void     rb_init(ringbuf_t *rb, uint8_t *mem, uint16_t size);
uint16_t rb_used(const ringbuf_t *rb);
uint16_t rb_free(const ringbuf_t *rb);
uint16_t rb_write(ringbuf_t *rb, const uint8_t *data, uint16_t len);
uint16_t rb_read(ringbuf_t *rb, uint8_t *data, uint16_t len);
uint8_t  rb_peek(const ringbuf_t *rb, uint16_t idx);
uint16_t rb_discard(ringbuf_t *rb, uint16_t len);

#endif
