/*
    本文件包含SHT30传感器数据的编解码函数实现。
    这些函数用于处理SHT30传感器的原始数据，包括CRC8校验和验证、温度和湿度数据的转换等。
*/

#include "sht30_codec.h"

/*
    函数作用：检查SHT30传感器数据的CRC8校验和
    - data: 待校验的数据
    - received_crc: 接收到的CRC8校验和
    - 返回值：如果校验和匹配，则返回true，否则返回false
*/
bool sht30_crc8_matches(const uint8_t data[2], uint8_t received_crc)
{
    uint8_t crc = 0xFFU;    // 初始化CRC8校验和为0xFFU

    for (uint8_t byte_index = 0U; byte_index < 2U; ++byte_index) {
        crc ^= data[byte_index];    // XOR操作
        for (uint8_t bit_index = 0U; bit_index < 8U; ++bit_index) {
            crc = (crc & 0x80U) != 0U ? (uint8_t)((crc << 1U) ^ 0x31U)
                                      : (uint8_t)(crc << 1U);
        }
    }

    return crc == received_crc;
}

/*
    函数作用：将SHT30传感器的原始温度数据转换为摄氏度的百分之一
    - raw_temperature: SHT30传感器的原始温度数据
    - 返回值：转换后的温度值，单位为摄氏度的百分之一
*/
int16_t sht30_raw_temperature_to_centi_c(uint16_t raw_temperature)
{
    const uint32_t scaled_temperature =
        ((17500U * raw_temperature) + (UINT16_MAX / 2U)) / UINT16_MAX;

    return (int16_t)scaled_temperature - 4500;
}

uint16_t sht30_raw_humidity_to_centi_pct(uint16_t raw_humidity)
{
    return (uint16_t)(((10000U * raw_humidity) + (UINT16_MAX / 2U)) /
                      UINT16_MAX);
}
