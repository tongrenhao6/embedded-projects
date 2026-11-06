/* ringbuf.c */
#include "ringbuf.h"

void rb_init(ringbuf_t *rb, uint8_t *mem, uint16_t size)
{
  rb->buf  = mem;
  rb->size = size;
  rb->head = 0;
  rb->tail = 0;
}

uint16_t rb_used(const ringbuf_t *rb)
{
  return (uint16_t)((rb->head - rb->tail) & (rb->size - 1));
}

uint16_t rb_free(const ringbuf_t *rb)
{
  return (uint16_t)(rb->size - 1 - rb_used(rb));
}

uint16_t rb_write(ringbuf_t *rb, const uint8_t *data, uint16_t len)
{
  uint16_t free_len = rb_free(rb);
  uint16_t i, n;
  n = (len < free_len) ? len : free_len;
  for (i = 0; i < n; i++)
  {
    rb->buf[rb->head] = data[i];
    rb->head = (uint16_t)((rb->head + 1) & (rb->size - 1));
  }
  return n;
}

uint16_t rb_read(ringbuf_t *rb, uint8_t *data, uint16_t len)
{
  uint16_t used = rb_used(rb);
  uint16_t i, n;
  n = (len < used) ? len : used;
  for (i = 0; i < n; i++)
  {
    data[i] = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1) & (rb->size - 1));
  }
  return n;
}

uint8_t rb_peek(const ringbuf_t *rb, uint16_t idx)
{
  return rb->buf[(rb->tail + idx) & (rb->size - 1)];
}

uint16_t rb_discard(ringbuf_t *rb, uint16_t len)
{
  uint16_t used = rb_used(rb);
  uint16_t n = (len < used) ? len : used;
  rb->tail = (uint16_t)((rb->tail + n) & (rb->size - 1));
  return n;
}
