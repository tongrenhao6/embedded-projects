#include "lora_service.h"

#include <inttypes.h>
#include <stdbool.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "app_config.h"
#include "lora_stream_parser.h"

#define LORA_SERVICE_UART_PORT UART_NUM_1
#define LORA_SERVICE_RX_BUFFER_SIZE 1024U
#define LORA_SERVICE_READ_BUFFER_SIZE 64U
#define LORA_SERVICE_READ_TIMEOUT_MS 1000U
#define LORA_SERVICE_TASK_STACK_SIZE 4096U
#define LORA_SERVICE_TASK_PRIORITY 5U

static const char *TAG = "lora_service";
static bool s_started;
static lora_service_frame_callback_t s_frame_callback;
static void *s_callback_context;
static lora_stream_parser_t s_parser;
static lora_service_stats_t s_stats;

static void lora_service_on_frame(const frame_codec_sensor_sample_t *sample,
                                  void *context)
{
    (void)context;
    s_frame_callback(sample, s_callback_context);
}

static void lora_service_log_parser_errors(void)
{
    const uint32_t crc_errors = lora_stream_parser_crc_error_count(&s_parser);
    const uint32_t invalid_frames =
        lora_stream_parser_invalid_frame_count(&s_parser);

    if (s_stats.crc_error_count != crc_errors ||
        s_stats.invalid_frame_count != invalid_frames) {
        ESP_LOGW(TAG,
                 "LoRa frame rejected: crc_errors=%" PRIu32
                 ", format_errors=%" PRIu32 ", discarded_bytes=%" PRIu32,
                 crc_errors, invalid_frames,
                 lora_stream_parser_discarded_byte_count(&s_parser));
    }
}

static void lora_service_receive_task(void *argument)
{
    (void)argument;
    uint8_t read_buffer[LORA_SERVICE_READ_BUFFER_SIZE];

    while (true) {
        const int bytes_read = uart_read_bytes(
            LORA_SERVICE_UART_PORT, read_buffer, sizeof(read_buffer),
            pdMS_TO_TICKS(LORA_SERVICE_READ_TIMEOUT_MS));
        if (bytes_read < 0) {
            ++s_stats.uart_read_error_count;
            ESP_LOGW(TAG, "UART read failed, errors=%" PRIu32,
                     s_stats.uart_read_error_count);
            vTaskDelay(pdMS_TO_TICKS(100U));
            continue;
        }
        if (bytes_read == 0) {
            continue;
        }

        s_stats.received_byte_count += (uint32_t)bytes_read;
        lora_service_log_parser_errors();
        lora_stream_parser_push(&s_parser, read_buffer, (size_t)bytes_read,
                                lora_service_on_frame, NULL);
        lora_service_log_parser_errors();

        s_stats.valid_frame_count = lora_stream_parser_valid_frame_count(&s_parser);
        s_stats.invalid_frame_count =
            lora_stream_parser_invalid_frame_count(&s_parser);
        s_stats.crc_error_count = lora_stream_parser_crc_error_count(&s_parser);
        s_stats.discarded_byte_count =
            lora_stream_parser_discarded_byte_count(&s_parser);
    }
}

esp_err_t lora_service_start(lora_service_frame_callback_t callback, void *context)
{
    if (callback == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }

    const uart_config_t uart_config = {
        .baud_rate = APP_LORA_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t result = uart_param_config(LORA_SERVICE_UART_PORT, &uart_config);
    if (result != ESP_OK) {
        return result;
    }

    result = uart_set_pin(LORA_SERVICE_UART_PORT, APP_LORA_UART_TX_GPIO,
                          APP_LORA_UART_RX_GPIO, UART_PIN_NO_CHANGE,
                          UART_PIN_NO_CHANGE);
    if (result != ESP_OK) {
        return result;
    }

    result = uart_driver_install(LORA_SERVICE_UART_PORT, LORA_SERVICE_RX_BUFFER_SIZE,
                                 0, 0, NULL, 0);
    if (result != ESP_OK) {
        return result;
    }

    lora_stream_parser_init(&s_parser);
    s_stats = (lora_service_stats_t){0};
    s_frame_callback = callback;
    s_callback_context = context;

    const BaseType_t task_created = xTaskCreate(
        lora_service_receive_task, "lora_rx", LORA_SERVICE_TASK_STACK_SIZE, NULL,
        LORA_SERVICE_TASK_PRIORITY, NULL);
    if (task_created != pdPASS) {
        uart_driver_delete(LORA_SERVICE_UART_PORT);
        s_frame_callback = NULL;
        s_callback_context = NULL;
        return ESP_ERR_NO_MEM;
    }

    s_started = true;
    ESP_LOGI(TAG, "DX-LR22 UART receiver started: UART1, %d bps, TX=%d, RX=%d",
             APP_LORA_UART_BAUD_RATE, APP_LORA_UART_TX_GPIO,
             APP_LORA_UART_RX_GPIO);
    return ESP_OK;
}

void lora_service_get_stats(lora_service_stats_t *stats)
{
    if (stats != NULL) {
        *stats = s_stats;
    }
}
