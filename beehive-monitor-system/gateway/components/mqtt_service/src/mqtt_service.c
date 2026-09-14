#include "mqtt_service.h"

#include <stdbool.h>
#include <string.h>

#include "esp_log.h"
#include "mqtt_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "mqtt_payload.h"

#define MQTT_SERVICE_QUEUE_LENGTH 8U
#define MQTT_SERVICE_TASK_STACK_SIZE 4096U
#define MQTT_SERVICE_TASK_PRIORITY 5U
#define MQTT_SERVICE_TASK_WAIT_MS 500U

static const char *TAG = "mqtt_service";
static esp_mqtt_client_handle_t s_client;
static QueueHandle_t s_sample_queue;
static TaskHandle_t s_publish_task;
static mqtt_service_stats_t s_stats;
static char s_gateway_id[MQTT_PAYLOAD_GATEWAY_ID_MAX_LENGTH];
static char s_status_topic[MQTT_PAYLOAD_TOPIC_MAX_LENGTH];
static volatile bool s_connected;
static bool s_started;

static void mqtt_service_publish_status(const char *status)
{
    const int message_id = esp_mqtt_client_publish(
        s_client, s_status_topic, status, 0, 1, 1);
    if (message_id < 0) {
        ESP_LOGW(TAG, "MQTT status publish failed: status=%s", status);
    } else {
        ESP_LOGI(TAG, "MQTT status published: status=%s", status);
    }
}

static void mqtt_service_event_handler(void *argument,
                                       esp_event_base_t event_base,
                                       int32_t event_id,
                                       void *event_data)
{
    (void)argument;
    (void)event_base;
    (void)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ++s_stats.connected_count;
        ESP_LOGI(TAG, "MQTT connected");
        mqtt_service_publish_status("online");
        if (s_publish_task != NULL) {
            xTaskNotifyGive(s_publish_task);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ++s_stats.disconnected_count;
        ESP_LOGW(TAG, "MQTT disconnected; telemetry queue retained in RAM");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT transport error");
        break;
    default:
        break;
    }
}

static void mqtt_service_requeue_sample(
    const frame_codec_sensor_sample_t *sample)
{
    if (xQueueSend(s_sample_queue, sample, 0U) != pdTRUE) {
        ++s_stats.queue_drop_count;
        ESP_LOGW(TAG, "MQTT telemetry queue full while reconnecting, sample dropped");
    }
}

static void mqtt_service_publish_task(void *argument)
{
    (void)argument;
    frame_codec_sensor_sample_t sample;
    char topic[MQTT_PAYLOAD_TOPIC_MAX_LENGTH];
    char json[MQTT_PAYLOAD_JSON_MAX_LENGTH];

    while (true) {
        if (!s_connected) {
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(MQTT_SERVICE_TASK_WAIT_MS));
            continue;
        }
        if (xQueueReceive(s_sample_queue, &sample,
                          pdMS_TO_TICKS(MQTT_SERVICE_TASK_WAIT_MS)) != pdTRUE) {
            continue;
        }
        if (!s_connected) {
            mqtt_service_requeue_sample(&sample);
            continue;
        }

        size_t topic_length;
        if (strcmp(CONFIG_BEEHIVE_MQTT_TOPIC_MODE, "bemfa") == 0) {
            topic_length = mqtt_payload_copy_topic(CONFIG_BEEHIVE_MQTT_TELEMETRY_TOPIC,
                                                   topic, sizeof(topic));
        } else {
            topic_length = mqtt_payload_format_telemetry_topic(
                CONFIG_BEEHIVE_MQTT_TOPIC_PREFIX, s_gateway_id, sample.node_id,
                topic, sizeof(topic));
        }
        const size_t json_length = mqtt_payload_format_telemetry_json(
            s_gateway_id, &sample, json, sizeof(json));
        if (topic_length == 0U || json_length == 0U) {
            ++s_stats.telemetry_publish_failure_count;
            ESP_LOGW(TAG, "MQTT telemetry formatting failed for node=%u",
                     (unsigned)sample.node_id);
            continue;
        }

        const int message_id = esp_mqtt_client_publish(
            s_client, topic, json, (int)json_length, 1, 0);
        if (message_id < 0) {
            ++s_stats.telemetry_publish_failure_count;
            ESP_LOGW(TAG, "MQTT telemetry publish failed: node=%u",
                     (unsigned)sample.node_id);
            if (!s_connected) {
                mqtt_service_requeue_sample(&sample);
            }
        } else {
            ++s_stats.telemetry_publish_success_count;
            ESP_LOGI(TAG, "MQTT telemetry published: node=%u, message_id=%d",
                     (unsigned)sample.node_id, message_id);
        }
    }
}

esp_err_t mqtt_service_start(void)
{
    if (s_started) {
        return ESP_ERR_INVALID_STATE;
    }
    if (CONFIG_BEEHIVE_MQTT_URI[0] == '\0' ||
        CONFIG_BEEHIVE_GATEWAY_ID[0] == '\0' ||
        CONFIG_BEEHIVE_MQTT_CLIENT_ID[0] == '\0' ||
        CONFIG_BEEHIVE_MQTT_TOPIC_PREFIX[0] == '\0') {
        ESP_LOGW(TAG, "MQTT is not configured; telemetry remains available on serial");
        return ESP_ERR_INVALID_STATE;
    }

    const size_t gateway_id_length = strlen(CONFIG_BEEHIVE_GATEWAY_ID);
    if (gateway_id_length >= sizeof(s_gateway_id)) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(s_gateway_id, CONFIG_BEEHIVE_GATEWAY_ID, gateway_id_length + 1U);
    size_t status_topic_length;
    if (strcmp(CONFIG_BEEHIVE_MQTT_TOPIC_MODE, "bemfa") == 0) {
        status_topic_length = mqtt_payload_copy_topic(CONFIG_BEEHIVE_MQTT_STATUS_TOPIC,
                                                      s_status_topic, sizeof(s_status_topic));
    } else {
        status_topic_length = mqtt_payload_format_status_topic(
                CONFIG_BEEHIVE_MQTT_TOPIC_PREFIX, s_gateway_id, s_status_topic,
                sizeof(s_status_topic));
    }
    if (status_topic_length == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(&s_stats, 0, sizeof(s_stats));
    s_sample_queue = xQueueCreate(MQTT_SERVICE_QUEUE_LENGTH,
                                   sizeof(frame_codec_sensor_sample_t));
    if (s_sample_queue == NULL) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(mqtt_service_publish_task, "mqtt_publish",
                    MQTT_SERVICE_TASK_STACK_SIZE, NULL,
                    MQTT_SERVICE_TASK_PRIORITY, &s_publish_task) != pdPASS) {
        vQueueDelete(s_sample_queue);
        s_sample_queue = NULL;
        return ESP_ERR_NO_MEM;
    }

    const char *username = CONFIG_BEEHIVE_MQTT_USERNAME[0] == '\0'
                               ? NULL
                               : CONFIG_BEEHIVE_MQTT_USERNAME;
    const char *password = CONFIG_BEEHIVE_MQTT_PASSWORD[0] == '\0'
                               ? NULL
                               : CONFIG_BEEHIVE_MQTT_PASSWORD;
    const esp_mqtt_client_config_t client_config = {
        .broker.address.uri = CONFIG_BEEHIVE_MQTT_URI,
        .credentials.username = username,
        .credentials.client_id = CONFIG_BEEHIVE_MQTT_CLIENT_ID,
        .credentials.authentication.password = password,
        .session.last_will.topic = s_status_topic,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 7,
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .session.disable_clean_session = false,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 3000,
        .network.timeout_ms = 10000,
    };

    s_client = esp_mqtt_client_init(&client_config);
    if (s_client == NULL) {
        vTaskDelete(s_publish_task);
        vQueueDelete(s_sample_queue);
        s_publish_task = NULL;
        s_sample_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = esp_mqtt_client_register_event(
        s_client, ESP_EVENT_ANY_ID, mqtt_service_event_handler, NULL);
    if (result == ESP_OK) {
        s_started = true;
        result = esp_mqtt_client_start(s_client);
    }
    if (result != ESP_OK) {
        s_started = false;
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
        vTaskDelete(s_publish_task);
        vQueueDelete(s_sample_queue);
        s_publish_task = NULL;
        s_sample_queue = NULL;
        return result;
    }

    ESP_LOGI(TAG, "MQTT service started: uri configured, client_id=%s, prefix=%s",
             CONFIG_BEEHIVE_MQTT_CLIENT_ID, CONFIG_BEEHIVE_MQTT_TOPIC_PREFIX);
    return ESP_OK;
}

esp_err_t mqtt_service_enqueue_sensor_sample(
    const frame_codec_sensor_sample_t *sample)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_started || s_sample_queue == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xQueueSend(s_sample_queue, sample, 0U) != pdTRUE) {
        ++s_stats.queue_drop_count;
        ESP_LOGW(TAG, "MQTT telemetry queue full, node=%u sample dropped",
                 (unsigned)sample->node_id);
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

bool mqtt_service_is_connected(void)
{
    return s_connected;
}

void mqtt_service_get_stats(mqtt_service_stats_t *stats)
{
    if (stats != NULL) {
        *stats = s_stats;
    }
}
