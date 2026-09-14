#include "ph_sensor_rs485.h"

#include <math.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "ph_config.h"

#define PH_SENSOR_UART_PORT     UART_NUM_1
#define PH_SENSOR_UART_TX       ((gpio_num_t)PH_RS485_UART_TX_GPIO)
#define PH_SENSOR_UART_RX       ((gpio_num_t)PH_RS485_UART_RX_GPIO)
#define PH_SENSOR_BAUD_RATE     9600
#define PH_SENSOR_READ_TIMEOUT  pdMS_TO_TICKS(500)
#define PH_SENSOR_SCAN_TIMEOUT  pdMS_TO_TICKS(120)
#define PH_SENSOR_TX_TIMEOUT    pdMS_TO_TICKS(50)
#define PH_SENSOR_RESPONSE_LEN  17
#define PH_SENSOR_RX_BUF_SIZE   40

static const char *TAG = "ph_sensor";
static bool s_uart_inited;
static const char *s_last_error = "INIT";
static uint8_t s_sensor_addr = 3;
static uint8_t s_scan_addr = 1;

const char *ph_sensor_get_last_error(void)
{
    return s_last_error;
}

static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (int bit = 0; bit < 8; bit++)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static float bytes_to_float(const uint8_t *src, int order)
{
    uint8_t bytes[4];
    float value;

    switch (order)
    {
        case 0:
            bytes[0] = src[0];
            bytes[1] = src[1];
            bytes[2] = src[2];
            bytes[3] = src[3];
            break;
        case 1:
            bytes[0] = src[1];
            bytes[1] = src[0];
            bytes[2] = src[3];
            bytes[3] = src[2];
            break;
        case 2:
            bytes[0] = src[2];
            bytes[1] = src[3];
            bytes[2] = src[0];
            bytes[3] = src[1];
            break;
        default:
            bytes[0] = src[3];
            bytes[1] = src[2];
            bytes[2] = src[1];
            bytes[3] = src[0];
            break;
    }

    memcpy(&value, bytes, sizeof(value));
    return value;
}

static bool decode_sensor_payload(const uint8_t *payload, ph_sensor_data_t *data)
{
    static const int order_priority[] = {3, 2, 1, 0};

    for (size_t i = 0; i < sizeof(order_priority) / sizeof(order_priority[0]); i++)
    {
        int order = order_priority[i];
        float ph = bytes_to_float(&payload[0], order);
        float mv = bytes_to_float(&payload[4], order);
        float temperature = bytes_to_float(&payload[8], order);

        if (isfinite(ph) && isfinite(mv) && isfinite(temperature) &&
            ph >= 0.0f && ph <= 14.0f &&
            mv >= -512.0f && mv <= 512.0f &&
            temperature >= -20.0f && temperature <= 100.0f)
        {
            data->ph = ph;
            data->mv = mv;
            data->temperature = temperature;
            return true;
        }
    }

    return false;
}

static const uint8_t *find_valid_response(const uint8_t *rx_buf, int rx_len, uint8_t addr)
{
    for (int offset = 0; offset <= rx_len - PH_SENSOR_RESPONSE_LEN; offset++)
    {
        const uint8_t *frame = &rx_buf[offset];
        uint16_t expected_crc;
        uint16_t actual_crc;

        if (frame[0] != addr || frame[1] != 0x03 || frame[2] != 0x0C)
        {
            continue;
        }

        expected_crc = modbus_crc16(frame, PH_SENSOR_RESPONSE_LEN - 2);
        actual_crc = frame[15] | (frame[16] << 8);
        if (expected_crc == actual_crc)
        {
            return frame;
        }
    }

    return NULL;
}

esp_err_t ph_sensor_init(void)
{
    if (s_uart_inited)
    {
        return ESP_OK;
    }

    const uart_config_t uart_config = {
        .baud_rate = PH_SENSOR_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_RETURN_ON_ERROR(uart_driver_install(PH_SENSOR_UART_PORT, 256, 0, 0, NULL, 0), TAG, "install uart failed");
    ESP_RETURN_ON_ERROR(uart_param_config(PH_SENSOR_UART_PORT, &uart_config), TAG, "config uart failed");
    ESP_RETURN_ON_ERROR(uart_set_pin(PH_SENSOR_UART_PORT, PH_SENSOR_UART_TX, PH_SENSOR_UART_RX,
                                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "set uart pin failed");

    s_uart_inited = true;
    ESP_LOGI(TAG, "uart%d init: tx=%d rx=%d baud=%d",
             PH_SENSOR_UART_PORT,
             PH_RS485_UART_TX_GPIO,
             PH_RS485_UART_RX_GPIO,
             PH_SENSOR_BAUD_RATE);
    return ESP_OK;
}

static esp_err_t read_addr(uint8_t addr, ph_sensor_data_t *data, TickType_t timeout_ticks, bool update_counter)
{
    uint8_t request[] = {addr, 0x03, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00};
    uint8_t rx_buf[PH_SENSOR_RX_BUF_SIZE] = {0};
    int rx_len = 0;
    const uint8_t *response;
    uint16_t request_crc;

    if (data == NULL)
    {
        s_last_error = "ARG";
        return ESP_ERR_INVALID_ARG;
    }

    ESP_RETURN_ON_ERROR(ph_sensor_init(), TAG, "init failed");

    request_crc = modbus_crc16(request, sizeof(request) - 2);
    request[6] = request_crc & 0xFF;
    request[7] = request_crc >> 8;
    
    uart_flush_input(PH_SENSOR_UART_PORT);
    ESP_LOG_BUFFER_HEX_LEVEL(TAG, request, sizeof(request), ESP_LOG_DEBUG);
    int tx_len = uart_write_bytes(PH_SENSOR_UART_PORT, request, sizeof(request));
    if (tx_len != sizeof(request))
    {
        if (update_counter)
        {
            data->err_count++;
        }
        data->online = false;
        s_last_error = "TX";
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(uart_wait_tx_done(PH_SENSOR_UART_PORT, PH_SENSOR_TX_TIMEOUT),
                        TAG, "wait tx done failed");

    while (rx_len < sizeof(rx_buf))
    {
        int chunk_len = uart_read_bytes(PH_SENSOR_UART_PORT,
                                        &rx_buf[rx_len],
                                        sizeof(rx_buf) - rx_len,
                                        rx_len == 0 ? timeout_ticks : pdMS_TO_TICKS(30));
        if (chunk_len <= 0)
        {
            break;
        }

        rx_len += chunk_len;
        if (find_valid_response(rx_buf, rx_len, addr) != NULL)
        {
            break;
        }
    }

    response = find_valid_response(rx_buf, rx_len, addr);
    if (response == NULL)
    {
        if (update_counter)
        {
            data->err_count++;
        }
        data->online = false;
        s_last_error = "TMO";
        ESP_LOGW(TAG, "addr=%u no valid response, len=%d", addr, rx_len);

        if (rx_len > 0)
        {
            ESP_LOG_BUFFER_HEX(TAG, rx_buf, rx_len);
        }

        return ESP_ERR_TIMEOUT;
    }

    ESP_LOG_BUFFER_HEX_LEVEL(TAG, response, PH_SENSOR_RESPONSE_LEN, ESP_LOG_INFO);

    uint16_t expected_crc = modbus_crc16(response, PH_SENSOR_RESPONSE_LEN - 2);
    uint16_t actual_crc = response[15] | (response[16] << 8);
    if (response[0] != addr ||
        response[1] != 0x03 ||
        response[2] != 0x0C)
    {
        if (update_counter)
        {
            data->err_count++;
        }
        data->online = false;
        s_last_error = "FRAME";
        ESP_LOGW(TAG, "bad header: %02X %02X %02X", response[0], response[1], response[2]);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (expected_crc != actual_crc)
    {
        if (update_counter)
        {
            data->err_count++;
        }
        data->online = false;
        s_last_error = "CRC";
        ESP_LOGW(TAG, "bad frame: %02X %02X %02X, crc=%04X/%04X",
                 response[0], response[1], response[2], actual_crc, expected_crc);
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!decode_sensor_payload(&response[3], data))
    {
        if (update_counter)
        {
            data->err_count++;
        }
        data->online = false;
        s_last_error = "FLOAT";
        ESP_LOGW(TAG, "float decode failed");
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (update_counter)
    {
        data->ok_count++;
    }
    data->address = addr;
    data->online = true;
    s_last_error = "OK";
    return ESP_OK;
}

esp_err_t ph_sensor_read(ph_sensor_data_t *data)
{
    esp_err_t ret;

    ret = read_addr(s_sensor_addr, data, PH_SENSOR_READ_TIMEOUT, true);
    if (ret == ESP_OK)
    {
        return ESP_OK;
    }

    for (int i = 0; i < 8; i++)
    {
        uint8_t addr = s_scan_addr++;

        if (s_scan_addr == 0)
        {
            s_scan_addr = 1;
        }

        if (addr == 0 || addr == s_sensor_addr)
        {
            continue;
        }

        ret = read_addr(addr, data, PH_SENSOR_SCAN_TIMEOUT, false);
        if (ret == ESP_OK)
        {
            s_sensor_addr = addr;
            data->ok_count++;
            data->address = addr;
            s_last_error = "FOUND";
            ESP_LOGI(TAG, "found sensor addr=%u", addr);
            return ESP_OK;
        }
    }

    s_last_error = "SCAN";
    return ESP_ERR_TIMEOUT;
}
