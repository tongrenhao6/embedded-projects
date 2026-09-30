#include "lora_transport.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "lora_payload.h"

#define LORA_UART_PORT UART_NUM_1
#define LORA_UART_BAUD_RATE 9600U
#define LORA_UART_TX_GPIO GPIO_NUM_4
#define LORA_UART_RX_GPIO GPIO_NUM_5
#define LORA_UART_BUFFER_SIZE 256U
#define LORA_UART_TX_TIMEOUT_MS 100U

static bool s_initialized;

esp_err_t lora_transport_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    const uart_config_t config = {
        .baud_rate = LORA_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t result = uart_param_config(LORA_UART_PORT, &config);
    if (result != ESP_OK) {
        return result;
    }

    result = uart_set_pin(LORA_UART_PORT, LORA_UART_TX_GPIO, LORA_UART_RX_GPIO,
                          UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (result != ESP_OK) {
        return result;
    }

    result = uart_driver_install(LORA_UART_PORT, LORA_UART_BUFFER_SIZE,
                                 LORA_UART_BUFFER_SIZE, 0, NULL, 0);
    if (result == ESP_OK) {
        s_initialized = true;
    }
    return result;
}

esp_err_t lora_transport_send(const uint8_t *payload, size_t payload_length)
{
    if (!lora_transport_payload_is_valid(payload, payload_length)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const int written = uart_write_bytes(LORA_UART_PORT, payload, payload_length);
    if (written < 0 || (size_t)written != payload_length) {
        return ESP_FAIL;
    }

    return uart_wait_tx_done(LORA_UART_PORT,
                             pdMS_TO_TICKS(LORA_UART_TX_TIMEOUT_MS));
}
