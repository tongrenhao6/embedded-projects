#include "wifi_service.h"

#include <inttypes.h>
#include <stdbool.h>
#include <string.h>

#include "app_config.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#define WIFI_SERVICE_CONNECTED_BIT BIT0

static const char *TAG = "wifi_service";
static EventGroupHandle_t s_wifi_event_group;
static bool s_started;
static bool s_connected;
static uint32_t s_retry_count;

static const char *wifi_service_disconnect_reason_name(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
        return "AP not found";
    case WIFI_REASON_AUTH_FAIL:
        return "authentication failed";
    case WIFI_REASON_ASSOC_FAIL:
        return "association failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return "WPA handshake timeout";
    case WIFI_REASON_BEACON_TIMEOUT:
        return "beacon timeout";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        return "four-way handshake timeout";
    default:
        return "other";
    }
}

static void wifi_service_event_handler(void *argument,
                                        esp_event_base_t event_base,
                                        int32_t event_id,
                                        void *event_data)
{
    (void)argument;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi STA started, connecting");
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *event =
            (const wifi_event_sta_disconnected_t *)event_data;
        s_connected = false;
        xEventGroupClearBits(s_wifi_event_group, WIFI_SERVICE_CONNECTED_BIT);
        ++s_retry_count;
        ESP_LOGW(TAG,
                 "Wi-Fi disconnected, reason=%u (%s), reconnect attempt=%" PRIu32,
                 event->reason, wifi_service_disconnect_reason_name(event->reason),
                 s_retry_count);
        esp_wifi_connect();
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        s_retry_count = 0U;
        s_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_SERVICE_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected, IP=" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t wifi_service_start(void)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (CONFIG_BEEHIVE_WIFI_SSID[0] == '\0') {
        ESP_LOGW(TAG, "Wi-Fi STA is not configured; LoRa will continue locally");
        return ESP_ERR_INVALID_STATE;
    }
    if (strlen(CONFIG_BEEHIVE_WIFI_SSID) >= sizeof(((wifi_config_t *)0)->sta.ssid) ||
        strlen(CONFIG_BEEHIVE_WIFI_PASSWORD) >= sizeof(((wifi_config_t *)0)->sta.password)) {
        ESP_LOGE(TAG, "Wi-Fi configuration exceeds ESP-IDF limits");
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = nvs_flash_init();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed for Wi-Fi: %s", esp_err_to_name(result));
        return result;
    }

    result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_ERR_NO_MEM;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    result = esp_wifi_init(&init_config);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        &wifi_service_event_handler, NULL);
    if (result == ESP_OK) {
        result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                            &wifi_service_event_handler, NULL);
    }
    if (result != ESP_OK) {
        return result;
    }

    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, CONFIG_BEEHIVE_WIFI_SSID,
           strlen(CONFIG_BEEHIVE_WIFI_SSID));
    memcpy(wifi_config.sta.password, CONFIG_BEEHIVE_WIFI_PASSWORD,
           strlen(CONFIG_BEEHIVE_WIFI_PASSWORD));
    wifi_config.sta.threshold.authmode =
        CONFIG_BEEHIVE_WIFI_PASSWORD[0] == '\0' ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;

    result = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (result == ESP_OK) {
        result = esp_wifi_set_mode(WIFI_MODE_STA);
    }
    if (result == ESP_OK) {
        result = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    }
    if (result == ESP_OK) {
        result = esp_wifi_start();
    }
    if (result != ESP_OK) {
        return result;
    }

    s_started = true;
    return ESP_OK;
}

bool wifi_service_is_connected(void)
{
    return s_connected;
}
