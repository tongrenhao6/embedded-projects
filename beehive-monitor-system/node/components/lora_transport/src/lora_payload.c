#include "lora_payload.h"

bool lora_transport_payload_is_valid(const uint8_t *payload, size_t payload_length)
{
    return payload != NULL && payload_length > 0U &&
           payload_length <= LORA_TRANSPORT_MAX_PAYLOAD_SIZE;
}
