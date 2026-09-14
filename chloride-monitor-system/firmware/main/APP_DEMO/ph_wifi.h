#ifndef __PH_WIFI_H
#define __PH_WIFI_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t ph_wifi_init(void);
bool ph_wifi_is_connected(void);
const char *ph_wifi_get_status(void);
bool ph_wifi_is_eth_connected(void);
bool ph_wifi_is_sta_connected(void);

#endif
