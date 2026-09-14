#include <stdint.h>
#include <stdio.h>

#include "sht30_codec.h"

#define ASSERT_TRUE(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                      \
            printf("Assertion failed: %s at %s:%d\n", #condition, __FILE__,  \
                   __LINE__);                                                   \
            return 1;                                                            \
        }                                                                        \
    } while (0)

static int test_validates_sensirion_crc8(void)
{
    const uint8_t data[2] = {0xBEU, 0xEFU};

    ASSERT_TRUE(sht30_crc8_matches(data, 0x92U));
    ASSERT_TRUE(!sht30_crc8_matches(data, 0x93U));
    return 0;
}

static int test_converts_temperature_to_centi_degrees_c(void)
{
    ASSERT_TRUE(sht30_raw_temperature_to_centi_c(0U) == -4500);
    ASSERT_TRUE(sht30_raw_temperature_to_centi_c(0x6666U) == 2500);
    ASSERT_TRUE(sht30_raw_temperature_to_centi_c(UINT16_MAX) == 13000);
    return 0;
}

static int test_converts_humidity_to_centi_percent(void)
{
    ASSERT_TRUE(sht30_raw_humidity_to_centi_pct(0U) == 0U);
    ASSERT_TRUE(sht30_raw_humidity_to_centi_pct(0x8000U) == 5000U);
    ASSERT_TRUE(sht30_raw_humidity_to_centi_pct(UINT16_MAX) == 10000U);
    return 0;
}

int main(void)
{
    int result = 0;

    result |= test_validates_sensirion_crc8();
    result |= test_converts_temperature_to_centi_degrees_c();
    result |= test_converts_humidity_to_centi_percent();

    if (result == 0) {
        printf("sht30 codec tests passed\n");
    }
    return result;
}
