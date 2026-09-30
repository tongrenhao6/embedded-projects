#include "board_rgb.h"

#include <stdlib.h>

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

#define BOARD_RGB_GPIO 48
#define BOARD_RGB_RMT_RESOLUTION_HZ 10000000U

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_handle_t bytes_encoder;
    rmt_encoder_handle_t copy_encoder;
    int state;
    rmt_symbol_word_t reset_code;
} ws2812_encoder_t;

static const char *TAG = "board_rgb";
static rmt_channel_handle_t s_tx_channel;
static rmt_encoder_handle_t s_encoder;

RMT_ENCODER_FUNC_ATTR
static size_t ws2812_encode(rmt_encoder_t *encoder,
                            rmt_channel_handle_t channel,
                            const void *primary_data,
                            size_t data_size,
                            rmt_encode_state_t *ret_state)
{
    ws2812_encoder_t *ws2812 = __containerof(encoder, ws2812_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;

    if (ws2812->state == 0) {
        encoded_symbols += ws2812->bytes_encoder->encode(ws2812->bytes_encoder,
                                                          channel,
                                                          primary_data,
                                                          data_size,
                                                          &session_state);
        if ((session_state & RMT_ENCODING_COMPLETE) != 0) {
            ws2812->state = 1;
        }
        if ((session_state & RMT_ENCODING_MEM_FULL) != 0) {
            *ret_state = RMT_ENCODING_MEM_FULL;
            return encoded_symbols;
        }
    }

    encoded_symbols += ws2812->copy_encoder->encode(ws2812->copy_encoder,
                                                     channel,
                                                     &ws2812->reset_code,
                                                     sizeof(ws2812->reset_code),
                                                     &session_state);
    if ((session_state & RMT_ENCODING_COMPLETE) != 0) {
        ws2812->state = RMT_ENCODING_RESET;
        state |= RMT_ENCODING_COMPLETE;
    }
    if ((session_state & RMT_ENCODING_MEM_FULL) != 0) {
        state |= RMT_ENCODING_MEM_FULL;
    }

    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t ws2812_del_encoder(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws2812 = __containerof(encoder, ws2812_encoder_t, base);

    rmt_del_encoder(ws2812->bytes_encoder);
    rmt_del_encoder(ws2812->copy_encoder);
    free(ws2812);
    return ESP_OK;
}

RMT_ENCODER_FUNC_ATTR
static esp_err_t ws2812_reset_encoder(rmt_encoder_t *encoder)
{
    ws2812_encoder_t *ws2812 = __containerof(encoder, ws2812_encoder_t, base);

    rmt_encoder_reset(ws2812->bytes_encoder);
    rmt_encoder_reset(ws2812->copy_encoder);
    ws2812->state = RMT_ENCODING_RESET;
    return ESP_OK;
}

static esp_err_t ws2812_new_encoder(rmt_encoder_handle_t *encoder)
{
    esp_err_t result = ESP_OK;
    ws2812_encoder_t *ws2812 = NULL;
    const rmt_bytes_encoder_config_t bytes_config = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 3,
            .level1 = 0,
            .duration1 = 9,
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 9,
            .level1 = 0,
            .duration1 = 3,
        },
        .flags.msb_first = 1,
    };
    const rmt_copy_encoder_config_t copy_config = {};

    ESP_RETURN_ON_FALSE(encoder != NULL, ESP_ERR_INVALID_ARG, TAG,
                        "encoder output is null");

    ws2812 = rmt_alloc_encoder_mem(sizeof(*ws2812));
    ESP_RETURN_ON_FALSE(ws2812 != NULL, ESP_ERR_NO_MEM, TAG,
                        "allocate WS2812 encoder failed");

    ws2812->base.encode = ws2812_encode;
    ws2812->base.del = ws2812_del_encoder;
    ws2812->base.reset = ws2812_reset_encoder;
    ws2812->reset_code = (rmt_symbol_word_t){
        .level0 = 0,
        .duration0 = 250,
        .level1 = 0,
        .duration1 = 250,
    };

    result = rmt_new_bytes_encoder(&bytes_config, &ws2812->bytes_encoder);
    if (result == ESP_OK) {
        result = rmt_new_copy_encoder(&copy_config, &ws2812->copy_encoder);
    }
    if (result != ESP_OK) {
        if (ws2812->bytes_encoder != NULL) {
            rmt_del_encoder(ws2812->bytes_encoder);
        }
        if (ws2812->copy_encoder != NULL) {
            rmt_del_encoder(ws2812->copy_encoder);
        }
        free(ws2812);
        return result;
    }

    *encoder = &ws2812->base;
    return ESP_OK;
}

esp_err_t board_rgb_init(void)
{
    const rmt_tx_channel_config_t channel_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = BOARD_RGB_GPIO,
        .mem_block_symbols = 64,
        .resolution_hz = BOARD_RGB_RMT_RESOLUTION_HZ,
        .trans_queue_depth = 1,
    };
    esp_err_t result;

    if (s_tx_channel != NULL) {
        return ESP_OK;
    }

    result = rmt_new_tx_channel(&channel_config, &s_tx_channel);
    if (result != ESP_OK) {
        return result;
    }
    result = ws2812_new_encoder(&s_encoder);
    if (result != ESP_OK) {
        rmt_del_channel(s_tx_channel);
        s_tx_channel = NULL;
        return result;
    }
    result = rmt_enable(s_tx_channel);
    if (result != ESP_OK) {
        rmt_del_encoder(s_encoder);
        rmt_del_channel(s_tx_channel);
        s_encoder = NULL;
        s_tx_channel = NULL;
        return result;
    }

    ESP_LOGI(TAG, "WS2812B RGB LED ready on GPIO%d", BOARD_RGB_GPIO);
    return ESP_OK;
}

esp_err_t board_rgb_set_rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    const uint8_t pixel[3] = {green, red, blue};
    const rmt_transmit_config_t transmit_config = {
        .loop_count = 0,
    };
    esp_err_t result;

    if (s_tx_channel == NULL || s_encoder == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    result = rmt_transmit(s_tx_channel, s_encoder, pixel, sizeof(pixel),
                          &transmit_config);
    if (result != ESP_OK) {
        return result;
    }
    return rmt_tx_wait_all_done(s_tx_channel, portMAX_DELAY);
}
