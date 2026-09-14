#ifndef __PH_TIME_H
#define __PH_TIME_H

#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

esp_err_t ph_time_init(void);
bool ph_time_is_valid(void);
time_t ph_time_now(void);
const char *ph_time_get_status(void);

#endif
