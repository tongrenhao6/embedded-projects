/* ota.h - OTA reserved interface (not implemented this stage) */
#ifndef OTA_H
#define OTA_H

#include <stdint.h>

#define OTA_RESULT_UNSUPPORTED  (-1)
#define OTA_RESULT_NO_UPDATE    (0)
#define OTA_RESULT_OK           (1)

void  ota_init(void);
int16_t ota_check(void);
int16_t ota_apply(void);

#endif
