/*
    This file implements the project Wi-Fi manager.
*/

#include "ph_wifi.h"

#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "ph_config.h"

#define PH_WIFI_CONNECTED_BIT BIT0

static const char *TAG = "ph_wifi";

static EventGroupHandle_t s_wifi_event_group;
static bool s_netif_ready;
static bool s_wifi_started;
static int s_retry_count;
static const char *s_wifi_status = "INIT";
static esp_netif_t *s_wifi_netif;

static esp_err_t ph_netif_init_once(void);
static void update_network_status(void);

static const char *ph_wifi_reason_to_str(uint8_t reason)
{
    switch (reason) {
        case WIFI_REASON_NO_AP_FOUND:
            return "NO_AP_FOUND";
        case WIFI_REASON_AUTH_FAIL:
            return "AUTH_FAIL";
        case WIFI_REASON_ASSOC_FAIL:
            return "ASSOC_FAIL";
        case WIFI_REASON_CONNECTION_FAIL:
            return "CONNECTION_FAIL";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "HANDSHAKE_TIMEOUT";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "BEACON_TIMEOUT";
        default:
            return "OTHER";
    }
}

static void update_network_status(void)
{
    EventBits_t bits = 0;

    if (s_wifi_event_group != NULL) {
        bits = xEventGroupGetBits(s_wifi_event_group);
    }

    if ((bits & PH_WIFI_CONNECTED_BIT) != 0) {
        s_wifi_status = "WIFI_CONNECTED";
    } else if (s_wifi_started) {
        s_wifi_status = "WIFI_WAIT";
    } else {
        s_wifi_status = "DISCONNECTED";
    }
}

static void ph_wifi_event_handler(void *arg, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base != WIFI_EVENT) {
        return;
    }

    if (event_id == WIFI_EVENT_STA_START) {
        s_wifi_status = "WIFI_CONNECTING";
        esp_wifi_connect();
        return;
    }

    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;

        xEventGroupClearBits(s_wifi_event_group, PH_WIFI_CONNECTED_BIT);
        s_retry_count++;
        s_wifi_status = "WIFI_RETRY";
        update_network_status();
        esp_wifi_connect();
        ESP_LOGW(TAG, "wifi disconnected, reason=%u(%s), retry=%d",
                 disconnected->reason,
                 ph_wifi_reason_to_str(disconnected->reason),
                 s_retry_count);
    }
}

static void ph_ip_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_base;

    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        s_retry_count = 0;
        xEventGroupSetBits(s_wifi_event_group, PH_WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "wifi got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        update_network_status();
    } else if (event_id == IP_EVENT_STA_LOST_IP) {
        xEventGroupClearBits(s_wifi_event_group, PH_WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "wifi lost ip");
        update_network_status();
    }
}

static esp_err_t ph_netif_init_once(void)
{
    esp_err_t ret;

    if (s_netif_ready) {
        return ESP_OK;
    }

    if (s_wifi_event_group == NULL) {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL) {
            s_wifi_status = "NO_MEM";
            return ESP_ERR_NO_MEM;
        }
    }

    ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT,
                                                   ESP_EVENT_ANY_ID,
                                                   &ph_wifi_event_handler,
                                                   NULL),
                        TAG, "register wifi event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT,
                                                   IP_EVENT_STA_GOT_IP,
                                                   &ph_ip_event_handler,
                                                   NULL),
                        TAG, "register sta got ip failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT,
                                                   IP_EVENT_STA_LOST_IP,
                                                   &ph_ip_event_handler,
                                                   NULL),
                        TAG, "register sta lost ip failed");

    s_netif_ready = true;
    return ESP_OK;
}

esp_err_t ph_wifi_init(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_config = {0};

    if (s_wifi_started) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(ph_netif_init_once(), TAG, "netif init failed");

    if (PH_WIFI_SSID[0] == '\0') {
        s_wifi_status = "NO_CONFIG";
        ESP_LOGW(TAG, "wifi ssid is empty, skip wifi init");
        return ESP_ERR_INVALID_STATE;
    }

    if (s_wifi_netif == NULL) {
        s_wifi_netif = esp_netif_create_default_wifi_sta();
        if (s_wifi_netif == NULL) {
            s_wifi_status = "NO_MEM";
            return ESP_ERR_NO_MEM;
        }
    }

    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");

    strlcpy((char *)wifi_config.sta.ssid, PH_WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, PH_WIFI_PASSWORD, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "wifi set mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_ps(WIFI_PS_NONE), TAG, "wifi set ps failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "wifi set config failed");
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");

    s_wifi_started = true;
    update_network_status();
    ESP_LOGI(TAG, "wifi sta started, ssid=%s", PH_WIFI_SSID);
    return ESP_OK;
}

bool ph_wifi_is_connected(void)
{
    if (s_wifi_event_group == NULL) {
        return false;
    }

    return (xEventGroupGetBits(s_wifi_event_group) & PH_WIFI_CONNECTED_BIT) != 0;
}

bool ph_wifi_is_eth_connected(void)
{
    return false;
}

bool ph_wifi_is_sta_connected(void)
{
    return ph_wifi_is_connected();
}

const char *ph_wifi_get_status(void)
{
    return s_wifi_status;
}
