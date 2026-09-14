#pragma once

#include <stdint.h>

#include "esp_err.h"

esp_err_t board_rgb_init(void);
esp_err_t board_rgb_set_rgb(uint8_t red, uint8_t green, uint8_t blue);
