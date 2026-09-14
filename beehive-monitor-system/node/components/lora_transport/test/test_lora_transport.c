#include <stdint.h>
#include <stdio.h>

#include "lora_payload.h"

#define ASSERT_TRUE(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                      \
            printf("Assertion failed: %s at %s:%d\n", #condition, __FILE__,  \
                   __LINE__);                                                   \
            return 1;                                                            \
        }                                                                        \
    } while (0)

static int test_accepts_sensor_frame_size(void)
{
    const uint8_t frame[26] = {0};

    ASSERT_TRUE(lora_transport_payload_is_valid(frame, sizeof(frame)));
    return 0;
}

static int test_rejects_empty_or_oversized_payload(void)
{
    const uint8_t oversized[LORA_TRANSPORT_MAX_PAYLOAD_SIZE + 1U] = {0};

    ASSERT_TRUE(!lora_transport_payload_is_valid(NULL, 1U));
    ASSERT_TRUE(!lora_transport_payload_is_valid(oversized, 0U));
    ASSERT_TRUE(!lora_transport_payload_is_valid(oversized, sizeof(oversized)));
    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_accepts_sensor_frame_size();
    result |= test_rejects_empty_or_oversized_payload();

    if (result == 0) {
        printf("lora_transport tests passed\n");
    }
    return result;
}
