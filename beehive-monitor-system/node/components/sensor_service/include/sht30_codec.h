#pragma once

#include <stdbool.h>
#include <stdint.h>

bool sht30_crc8_matches(const uint8_t data[2], uint8_t received_crc);
int16_t sht30_raw_temperature_to_centi_c(uint16_t raw_temperature);
uint16_t sht30_raw_humidity_to_centi_pct(uint16_t raw_humidity);
