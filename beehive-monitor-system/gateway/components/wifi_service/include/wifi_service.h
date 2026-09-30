#pragma once

#include <stdbool.h>

#include "esp_err.h"

esp_err_t wifi_service_start(void);

bool wifi_service_is_connected(void);
