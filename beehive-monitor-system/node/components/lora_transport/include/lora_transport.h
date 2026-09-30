#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define LORA_TRANSPORT_MAX_PAYLOAD_SIZE 230U

esp_err_t lora_transport_init(void);

esp_err_t lora_transport_send(const uint8_t *payload, size_t payload_length);
