/* log_service.h - lightweight tag logger via printf (USART1) */
#ifndef LOG_SERVICE_H
#define LOG_SERVICE_H

#include <stdint.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 3U   /* 0=off 1=err 2=warn 3=info 4=debug */
#endif

#define LOG_LEVEL_OFF   0U
#define LOG_LEVEL_ERR   1U
#define LOG_LEVEL_WARN  2U
#define LOG_LEVEL_INFO  3U
#define LOG_LEVEL_DEBUG 4U

void log_init(void);
void log_msg(const char *tag, const char *fmt, ...);

#endif
