/* log_service.c */
#include "log_service.h"
#include "bsp_rtc.h"
#include <stdio.h>
#include <stdarg.h>

void log_init(void)
{
  /* USART1 print path lives in fputc (main.c) */
}

void log_msg(const char *tag, const char *fmt, ...)
{
  va_list ap;
#if (LOG_LEVEL >= LOG_LEVEL_INFO)
  uint32_t ts = bsp_rtc_timestamp();
  printf("[%lu]", (unsigned long)ts);
  if (tag != 0) printf("[%s]", tag);
  printf(" ");
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\r\n");
#else
  (void)tag;
  (void)fmt;
  va_start(ap, fmt);
  va_end(ap);
#endif
}
