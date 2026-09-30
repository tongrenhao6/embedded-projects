#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LORA_TRANSPORT_MAX_PAYLOAD_SIZE 230U

bool lora_transport_payload_is_valid(const uint8_t *payload, size_t payload_length);
