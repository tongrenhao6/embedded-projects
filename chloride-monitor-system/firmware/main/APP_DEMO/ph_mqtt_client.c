#include "ph_mqtt_client.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "ff.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "ph_config.h"
#include "ph_storage_sd.h"
#include "ph_time.h"
#include "ph_wifi.h"

#define PHLOG_CURRENT_DIR       "0:/PHLOG/data/current"
#define MQTT_TOPIC_MAX          128
#define MQTT_JSON_MAX           384
#define CSV_LINE_MAX            384
#define MQTT_CMD_NAME_MAX       32
#define MQTT_CMD_MONTH_MAX      8
#define MQTT_CMD_QUEUE_LEN      4
#define MQTT_PUBLISH_TIMEOUT_MS 8000
#define MQTT_CONNECTED_BIT      BIT0
#define MQTT_PUBLISHED_BIT      BIT1
#define MQTT_CLIENT_TASK_STACK  10240
#define MQTT_STATUS_TASK_STACK  12288
#define MQTT_COMMAND_TASK_STACK 12288

typedef struct {
    int command_id;
    char command_name[MQTT_CMD_NAME_MAX];
    char month[MQTT_CMD_MONTH_MAX];
    bool force_replay;
} ph_mqtt_command_t;

static const char *TAG = "ph_mqtt";
static esp_mqtt_client_handle_t s_client;
static EventGroupHandle_t s_event_group;
static SemaphoreHandle_t s_publish_mutex;
static SemaphoreHandle_t s_upload_mutex;
static QueueHandle_t s_command_queue;
static TaskHandle_t s_command_task;
static TaskHandle_t s_status_task;
static int s_wait_msg_id = -1;
static char s_status_topic[MQTT_TOPIC_MAX];
static char s_last_error[64] = "INIT";
static char s_status[32] = "INIT";
static char s_last_upload_at[20] = "";
static uint32_t s_upload_failed_count;

static esp_err_t publish_upload_status_current(bool wait_for_ack, const char *last_error);
static void upload_status_task(void *arg);

const char *ph_mqtt_client_get_last_error(void)
{
    return s_last_error;
}

const char *ph_mqtt_client_get_status(void)
{
    return s_status;
}

bool ph_mqtt_client_is_connected(void)
{
    return s_event_group != NULL &&
           (xEventGroupGetBits(s_event_group) & MQTT_CONNECTED_BIT) != 0;
}

static void build_topic(const char *suffix, char *topic, size_t topic_size)
{
    snprintf(topic, topic_size, "%s/devices/%s/%s",
             PH_MQTT_BASE_TOPIC, PH_DEVICE_ID, suffix);
}

static void build_month_file_name(int year, int month, char *file_name, size_t file_name_size)
{
    snprintf(file_name, file_name_size, "ph_%04d_%02d.csv", year, month);
}

static void format_timestamp(time_t timestamp, bool time_valid,
                             char *text, size_t text_size)
{
    struct tm timeinfo;

    if (!time_valid) {
        snprintf(text, text_size, "unknown");
        return;
    }

    localtime_r(&timestamp, &timeinfo);
    snprintf(text, text_size, "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

static void update_last_upload_at(void)
{
    if (!ph_time_is_valid()) {
        s_last_upload_at[0] = '\0';
        return;
    }

    format_timestamp(ph_time_now(), true, s_last_upload_at, sizeof(s_last_upload_at));
}

static void json_escape_text(const char *src, char *dst, size_t dst_size)
{
    size_t pos = 0;

    if (dst == NULL || dst_size == 0) {
        return;
    }
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    while (*src != '\0' && pos < dst_size - 1) {
        char ch = *src++;

        if ((ch == '"' || ch == '\\') && pos < dst_size - 2) {
            dst[pos++] = '\\';
            dst[pos++] = ch;
        } else if ((unsigned char)ch >= 0x20) {
            dst[pos++] = ch;
        }
    }

    dst[pos] = '\0';
}

static bool read_csv_line(FIL *fp, char *line, size_t line_size)
{
    UINT read_len;
    size_t pos = 0;
    char ch;

    if (line_size == 0) {
        return false;
    }

    while (pos < line_size - 1) {
        if (f_read(fp, &ch, 1, &read_len) != FR_OK || read_len == 0) {
            break;
        }

        line[pos++] = ch;
        if (ch == '\n') {
            break;
        }
    }

    line[pos] = '\0';
    return pos > 0;
}

static bool is_data_line(const char *line)
{
    if (line == NULL || line[0] == '\0' || line[0] == '\r' || line[0] == '\n') {
        return false;
    }

    return strncmp(line, "date,", 5) != 0;
}

static bool parse_sample_line(char *line, char *date, size_t date_size,
                              char *time_text, size_t time_size,
                              float *ph, float *temperature)
{
    char *saveptr = NULL;
    char *field;

    field = strtok_r(line, ",\r\n", &saveptr);
    if (field == NULL) {
        return false;
    }
    snprintf(date, date_size, "%s", field);

    field = strtok_r(NULL, ",\r\n", &saveptr);
    if (field == NULL) {
        return false;
    }
    snprintf(time_text, time_size, "%s", field);

    field = strtok_r(NULL, ",\r\n", &saveptr);
    if (field == NULL) {
        return false;
    }
    *ph = strtof(field, NULL);

    field = strtok_r(NULL, ",\r\n", &saveptr);
    if (field == NULL) {
        return false;
    }
    *temperature = strtof(field, NULL);

    return true;
}

static void format_sample_datetime(const ph_sample_t *sample,
                                   char *date, size_t date_size,
                                   char *time_text, size_t time_size)
{
    struct tm timeinfo;
    char timestamp[20];

    if (sample == NULL || !sample->time_valid) {
        snprintf(date, date_size, "unknown");
        snprintf(time_text, time_size, "unknown");
        return;
    }

    format_timestamp(sample->timestamp, sample->time_valid,
                     timestamp, sizeof(timestamp));
    if (sscanf(timestamp, "%10s %8s", date, time_text) != 2) {
        localtime_r(&sample->timestamp, &timeinfo);
        snprintf(date, date_size, "%04d-%02d-%02d",
                 timeinfo.tm_year + 1900,
                 timeinfo.tm_mon + 1,
                 timeinfo.tm_mday);
        snprintf(time_text, time_size, "%02d:%02d:%02d",
                 timeinfo.tm_hour,
                 timeinfo.tm_min,
                 timeinfo.tm_sec);
    }
}

static esp_err_t mqtt_publish_wait(const char *topic, const char *payload,
                                   int qos, int retain, int timeout_ms)
{
    int msg_id;
    EventBits_t bits;

    if (s_client == NULL || !ph_mqtt_client_is_connected()) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT_OFFLINE");
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_publish_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT_BUSY");
        return ESP_ERR_TIMEOUT;
    }

    xEventGroupClearBits(s_event_group, MQTT_PUBLISHED_BIT);
    s_wait_msg_id = -1;
    msg_id = esp_mqtt_client_publish(s_client, topic, payload, 0, qos, retain);
    if (msg_id < 0) {
        s_wait_msg_id = -1;
        xSemaphoreGive(s_publish_mutex);
        snprintf(s_last_error, sizeof(s_last_error), "PUB:%d", msg_id);
        ESP_LOGW(TAG, "publish failed topic=%s msg_id=%d", topic, msg_id);
        return ESP_FAIL;
    }

    if (qos == 0) {
        ESP_LOGI(TAG, "publish queued topic=%s msg_id=%d qos=0", topic, msg_id);
        xSemaphoreGive(s_publish_mutex);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "publish wait topic=%s msg_id=%d", topic, msg_id);
    s_wait_msg_id = msg_id;
    bits = xEventGroupWaitBits(s_event_group,
                               MQTT_PUBLISHED_BIT,
                               pdTRUE,
                               pdFALSE,
                               pdMS_TO_TICKS(timeout_ms));
    s_wait_msg_id = -1;
    xSemaphoreGive(s_publish_mutex);

    if ((bits & MQTT_PUBLISHED_BIT) == 0) {
        snprintf(s_last_error, sizeof(s_last_error), "PUB_TIMEOUT");
        ESP_LOGW(TAG, "publish timeout topic=%s msg_id=%d", topic, msg_id);
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "publish ack topic=%s msg_id=%d", topic, msg_id);
    return ESP_OK;
}

static esp_err_t publish_command_result(int command_id, const char *status, const char *result)
{
    char topic[MQTT_TOPIC_MAX];
    char json[MQTT_JSON_MAX];
    char escaped_result[128];
    int len;

    json_escape_text(result, escaped_result, sizeof(escaped_result));
    build_topic("commands/up", topic, sizeof(topic));
    len = snprintf(json, sizeof(json),
                   "{\"command_id\":%d,\"status\":\"%s\",\"result\":\"%s\"}",
                   command_id, status, escaped_result);
    if (len <= 0 || len >= sizeof(json)) {
        snprintf(s_last_error, sizeof(s_last_error), "CMD_JSON_LONG");
        return ESP_FAIL;
    }

    return mqtt_publish_wait(topic, json, 1, 0, MQTT_PUBLISH_TIMEOUT_MS);
}

static esp_err_t publish_sample(const char *date, const char *time_text,
                                float ph, float temperature, const char *source)
{
    char topic[MQTT_TOPIC_MAX];
    char json[MQTT_JSON_MAX];
    char sample_time[32];
    int len;

    snprintf(sample_time, sizeof(sample_time), "%s %s", date, time_text);
    build_topic("telemetry", topic, sizeof(topic));
    len = snprintf(json, sizeof(json),
                   "{\"device_id\":\"%s\",\"sample_time\":\"%s\","
                   "\"date\":\"%s\",\"time\":\"%s\","
                   "\"ph\":%.2f,\"temperature\":%.1f,\"source\":\"%s\","
                   "\"readings\":{\"ph\":{\"value\":%.2f,\"unit\":\"pH\"},"
                   "\"temperature\":{\"value\":%.1f,\"unit\":\"C\"}}}",
                   PH_DEVICE_ID, sample_time, date, time_text,
                   ph, temperature, source, ph, temperature);
    if (len <= 0 || len >= sizeof(json)) {
        snprintf(s_last_error, sizeof(s_last_error), "JSON_LONG");
        return ESP_FAIL;
    }

    return mqtt_publish_wait(topic, json, 1, 0, MQTT_PUBLISH_TIMEOUT_MS);
}

static esp_err_t publish_upload_status_for_month(int year, int month,
                                                 bool wait_for_ack,
                                                 const char *last_error)
{
    char topic[MQTT_TOPIC_MAX];
    char json[MQTT_JSON_MAX];
    char oldest_time[20] = "";
    char escaped_error[80] = "";
    uint32_t pending_count = 0;
    int len;
    esp_err_t ret;

    (void)year;
    (void)month;

    if (!ph_mqtt_client_is_connected()) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT:%s", s_status);
        return ESP_ERR_INVALID_STATE;
    }

    ret = ph_storage_get_pending_upload_summary(&pending_count, oldest_time, sizeof(oldest_time));
    if (ret != ESP_OK) {
        pending_count = 0;
        if (last_error == NULL || last_error[0] == '\0') {
            last_error = ph_storage_get_last_error();
        }
    }
    json_escape_text(last_error, escaped_error, sizeof(escaped_error));

    build_topic("upload/status", topic, sizeof(topic));
    len = snprintf(json, sizeof(json),
                   "{\"pending_count\":%lu,\"failed_count\":%lu,"
                   "\"oldest_sample_time\":%s%s%s,"
                   "\"last_upload_at\":%s%s%s,"
                   "\"last_error\":%s%s%s}",
                   (unsigned long)pending_count,
                   (unsigned long)s_upload_failed_count,
                   oldest_time[0] != '\0' ? "\"" : "",
                   oldest_time[0] != '\0' ? oldest_time : "null",
                   oldest_time[0] != '\0' ? "\"" : "",
                   s_last_upload_at[0] != '\0' ? "\"" : "",
                   s_last_upload_at[0] != '\0' ? s_last_upload_at : "null",
                   s_last_upload_at[0] != '\0' ? "\"" : "",
                   escaped_error[0] != '\0' ? "\"" : "",
                   escaped_error[0] != '\0' ? escaped_error : "null",
                   escaped_error[0] != '\0' ? "\"" : "");
    if (len <= 0 || len >= sizeof(json)) {
        snprintf(s_last_error, sizeof(s_last_error), "STATUS_JSON_LONG");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "upload status: pending=%lu failed=%lu oldest=%s last_error=%s",
             (unsigned long)pending_count,
             (unsigned long)s_upload_failed_count,
             oldest_time[0] != '\0' ? oldest_time : "null",
             escaped_error[0] != '\0' ? escaped_error : "null");

    if (wait_for_ack) {
        return mqtt_publish_wait(topic, json, 1, 0, MQTT_PUBLISH_TIMEOUT_MS);
    }

    if (esp_mqtt_client_publish(s_client, topic, json, 0, 1, 0) < 0) {
        snprintf(s_last_error, sizeof(s_last_error), "STATUS_PUB");
        return ESP_FAIL;
    }

    return ESP_OK;
}

static esp_err_t publish_upload_status_current(bool wait_for_ack, const char *last_error)
{
    return publish_upload_status_for_month(0, 0, wait_for_ack, last_error);
}

esp_err_t ph_mqtt_publish_upload_status_current(const char *last_error)
{
    return publish_upload_status_current(true, last_error);
}

static void request_upload_status_async(void)
{
    if (s_status_task != NULL) {
        xTaskNotifyGive(s_status_task);
    }
}

static void upload_status_task(void *arg)
{
    (void)arg;

    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(500));
        publish_upload_status_current(true, NULL);
    }
}

esp_err_t ph_mqtt_publish_sample_direct(const ph_sample_t *sample, const char *source)
{
    char date[16];
    char time_text[16];

    if (sample == NULL) {
        snprintf(s_last_error, sizeof(s_last_error), "ARG");
        return ESP_ERR_INVALID_ARG;
    }
    if (source == NULL || source[0] == '\0') {
        source = "direct";
    }
    if (!ph_wifi_is_connected()) {
        snprintf(s_last_error, sizeof(s_last_error), "WIFI:%s", ph_wifi_get_status());
        return ESP_ERR_INVALID_STATE;
    }
    if (!ph_mqtt_client_is_connected()) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT:%s", s_status);
        return ESP_ERR_INVALID_STATE;
    }

    format_sample_datetime(sample, date, sizeof(date), time_text, sizeof(time_text));
    esp_err_t ret = publish_sample(date, time_text, sample->ph, sample->temperature, source);
    if (ret == ESP_OK) {
        update_last_upload_at();
    } else {
        s_upload_failed_count++;
    }

    return ret;
}

static esp_err_t upload_month_internal(int year, int month, bool force_replay,
                                       uint32_t *out_uploaded_count)
{
    FIL *fp;
    FRESULT fres;
    char *line;
    char current_path[64];
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    ph_upload_cursor_t cursor;
    uint32_t uploaded = 0;
    uint32_t data_index = 0;
    uint32_t uploaded_line = 0;
    esp_err_t ret = ESP_OK;

    if (out_uploaded_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_uploaded_count = 0;

    if (year < 2024 || month < 1 || month > 12) {
        snprintf(s_last_error, sizeof(s_last_error), "BAD_MONTH");
        return ESP_ERR_INVALID_ARG;
    }
    if (PH_MQTT_BROKER_URI[0] == '\0') {
        snprintf(s_last_error, sizeof(s_last_error), "NO_MQTT_URI");
        return ESP_ERR_INVALID_STATE;
    }
    if (!ph_wifi_is_connected()) {
        snprintf(s_last_error, sizeof(s_last_error), "WIFI:%s", ph_wifi_get_status());
        return ESP_ERR_INVALID_STATE;
    }
    if (!ph_mqtt_client_is_connected()) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT:%s", s_status);
        return ESP_ERR_INVALID_STATE;
    }
    if (ph_storage_init() != ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "SD:%s", ph_storage_get_last_error());
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "upload_month: starting %04d-%02d force=%d", year, month, force_replay ? 1 : 0);
    publish_upload_status_for_month(year, month, true, NULL);
    if (xSemaphoreTake(s_upload_mutex, pdMS_TO_TICKS(10000)) != pdTRUE) {
        snprintf(s_last_error, sizeof(s_last_error), "UPLOAD_BUSY");
        s_upload_failed_count++;
        publish_upload_status_for_month(year, month, true, s_last_error);
        return ESP_ERR_TIMEOUT;
    }

    build_month_file_name(year, month, file_name, sizeof(file_name));
    memset(&cursor, 0, sizeof(cursor));
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
    if (ph_upload_cursor_load(&cursor) != ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "CURSOR:%s", ph_storage_get_last_error());
        xSemaphoreGive(s_upload_mutex);
        ESP_LOGE(TAG, "upload_month: cursor load failed for %s", file_name);
        return ESP_FAIL;
    }
    uploaded_line = force_replay ? 0 : cursor.line;
    ESP_LOGI(TAG, "upload_month: %s cursor.line=%lu start_line=%lu",
             file_name,
             (unsigned long)cursor.line,
             (unsigned long)uploaded_line);

    snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/%s", file_name);
    fp = (FIL *)malloc(sizeof(FIL));
    line = (char *)malloc(CSV_LINE_MAX);
    if (fp == NULL || line == NULL) {
        free(fp);
        free(line);
        snprintf(s_last_error, sizeof(s_last_error), "NO_MEM");
        xSemaphoreGive(s_upload_mutex);
        return ESP_ERR_NO_MEM;
    }

    fres = f_open(fp, current_path, FA_READ | FA_OPEN_EXISTING);
    if (fres == FR_NO_FILE) {
        free(fp);
        free(line);
        update_last_upload_at();
        s_upload_failed_count = 0;
        snprintf(s_last_error, sizeof(s_last_error), "OK");
        ESP_LOGW(TAG, "upload_month: file not found %s", current_path);
        publish_upload_status_for_month(year, month, true, NULL);
        xSemaphoreGive(s_upload_mutex);
        return ESP_OK;
    }
    if (fres != FR_OK) {
        free(fp);
        free(line);
        snprintf(s_last_error, sizeof(s_last_error), "OPEN:%d", (int)fres);
        xSemaphoreGive(s_upload_mutex);
        return ESP_FAIL;
    }

    while (read_csv_line(fp, line, CSV_LINE_MAX)) {
        char date[16];
        char time_text[16];
        float ph;
        float temperature;

        if (!is_data_line(line)) {
            continue;
        }

        data_index++;
        if (data_index <= uploaded_line) {
            continue;
        }

        if (!parse_sample_line(line, date, sizeof(date), time_text, sizeof(time_text), &ph, &temperature)) {
            snprintf(s_last_error, sizeof(s_last_error), "CSV:%lu", (unsigned long)data_index);
            ret = ESP_FAIL;
            break;
        }

        ret = publish_sample(date, time_text, ph, temperature, "backfill");
        if (ret != ESP_OK) {
            s_upload_failed_count++;
            publish_upload_status_for_month(year, month, true, s_last_error);
            break;
        }

        if (data_index > cursor.line) {
            snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
            cursor.line = data_index;
            if (ph_upload_cursor_save(&cursor) != ESP_OK) {
                snprintf(s_last_error, sizeof(s_last_error), "CURSOR_SAVE");
                s_upload_failed_count++;
                publish_upload_status_for_month(year, month, true, s_last_error);
                ret = ESP_FAIL;
                break;
            }
        }

        update_last_upload_at();
        uploaded++;
        publish_upload_status_for_month(year, month, true, NULL);
        ESP_LOGI(TAG, "uploaded %s %s ph=%.2f temp=%.1f", date, time_text, ph, temperature);
    }

    f_close(fp);
    free(fp);
    free(line);
    xSemaphoreGive(s_upload_mutex);

    *out_uploaded_count = uploaded;
    ESP_LOGI(TAG, "upload_month: %s done, data_lines=%lu uploaded=%lu, ret=%d",
             file_name, (unsigned long)data_index, (unsigned long)uploaded, (int)ret);
    if (ret == ESP_OK) {
        s_upload_failed_count = 0;
        update_last_upload_at();
        snprintf(s_last_error, sizeof(s_last_error), "OK");
        publish_upload_status_for_month(year, month, true, NULL);
    }

    return ret;
}

esp_err_t ph_mqtt_upload_month(int year, int month, uint32_t *out_uploaded_count)
{
    return upload_month_internal(year, month, false, out_uploaded_count);
}

static bool parse_command_month(const char *month_text, int *year, int *month)
{
    int y;
    int m;

    if (month_text == NULL || year == NULL || month == NULL) {
        return false;
    }
    if (strlen(month_text) != 7 || month_text[4] != '-') {
        return false;
    }
    if (month_text[0] < '0' || month_text[0] > '9' ||
        month_text[1] < '0' || month_text[1] > '9' ||
        month_text[2] < '0' || month_text[2] > '9' ||
        month_text[3] < '0' || month_text[3] > '9' ||
        month_text[5] < '0' || month_text[5] > '9' ||
        month_text[6] < '0' || month_text[6] > '9') {
        return false;
    }
    if (sscanf(month_text, "%4d-%2d", &y, &m) != 2) {
        return false;
    }
    if (y < 2024 || m < 1 || m > 12) {
        return false;
    }

    *year = y;
    *month = m;
    return true;
}

static void format_command_month(int year, int month, char *text, size_t text_size)
{
    if (text == NULL || text_size == 0) {
        return;
    }

    snprintf(text, text_size, "%04d-%02d", year, month);
}

static const char *describe_upload_error(const char *error_text)
{
    if (error_text == NULL || error_text[0] == '\0') {
        return "upload failed";
    }
    if (strcmp(error_text, "BAD_MONTH") == 0) {
        return "invalid month";
    }
    if (strcmp(error_text, "PUB_TIMEOUT") == 0) {
        return "network timeout";
    }
    if (strncmp(error_text, "WIFI:", 5) == 0) {
        return "wifi disconnected";
    }
    if (strncmp(error_text, "MQTT:", 5) == 0 ||
        strcmp(error_text, "MQTT_OFFLINE") == 0 ||
        strcmp(error_text, "MQTT_BUSY") == 0 ||
        strcmp(error_text, "MQTT_ERROR") == 0 ||
        strncmp(error_text, "PUB:", 4) == 0) {
        return "mqtt publish failed";
    }
    if (strncmp(error_text, "OPEN:", 5) == 0 ||
        strncmp(error_text, "UPLOAD_OPEN", 11) == 0 ||
        strncmp(error_text, "DIR_OPEN", 8) == 0 ||
        strncmp(error_text, "DIR_READ", 8) == 0 ||
        strncmp(error_text, "SD:", 3) == 0) {
        return "sd read failed";
    }
    if (strncmp(error_text, "CURSOR", 6) == 0) {
        return "upload cursor failed";
    }

    return error_text;
}

static void command_task(void *arg)
{
    ph_mqtt_command_t cmd;

    (void)arg;

    while (1) {
        if (xQueueReceive(s_command_queue, &cmd, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        ESP_LOGI(TAG, "got command id=%d name=%s", cmd.command_id, cmd.command_name);

        if (strcmp(cmd.command_name, "UPLOAD_PENDING") == 0) {
            int year;
            int month;
            uint32_t uploaded = 0;
            esp_err_t ret;
            char month_text[MQTT_CMD_MONTH_MAX];
            char result[96];

            if (!parse_command_month(cmd.month, &year, &month)) {
                ESP_LOGW(TAG, "command %d: invalid month '%s'", cmd.command_id, cmd.month);
                publish_command_result(cmd.command_id, "failed", "invalid month");
                continue;
            }
            format_command_month(year, month, month_text, sizeof(month_text));
            ESP_LOGI(TAG, "command %d: UPLOAD_PENDING for %s", cmd.command_id, month_text);

            snprintf(result, sizeof(result), "upload pending started for %s", month_text);
            if (ph_mqtt_client_is_connected()) {
                ESP_LOGI(TAG, "command %d: publishing running ack", cmd.command_id);
                publish_command_result(cmd.command_id, "running", result);
                publish_upload_status_for_month(year, month, true, NULL);
            } else {
                ESP_LOGW(TAG, "command %d: MQTT offline, skipping pre-upload status", cmd.command_id);
            }

            ESP_LOGI(TAG, "command %d: starting upload for %s force=%d",
                     cmd.command_id, month_text, cmd.force_replay ? 1 : 0);
            ret = upload_month_internal(year, month, cmd.force_replay, &uploaded);
            ESP_LOGI(TAG, "command %d: upload done, ret=%d (%s) uploaded=%lu",
                     cmd.command_id, (int)ret, esp_err_to_name(ret),
                     (unsigned long)uploaded);

            if (ret == ESP_OK) {
                snprintf(result, sizeof(result), "uploaded %lu samples for %s",
                         (unsigned long)uploaded,
                         month_text);
                ESP_LOGI(TAG, "command %d: publishing done: %s", cmd.command_id, result);
                publish_command_result(cmd.command_id, "done", result);
            } else {
                snprintf(result, sizeof(result), "%s for %s",
                         describe_upload_error(s_last_error),
                         month_text);
                ESP_LOGW(TAG, "command %d: upload failed: %s", cmd.command_id, result);
                publish_command_result(cmd.command_id, "failed", result);
            }
            publish_upload_status_for_month(year, month, true,
                                            ret == ESP_OK ? NULL : s_last_error);
        } else {
            ESP_LOGW(TAG, "command %d: unknown command '%s'", cmd.command_id, cmd.command_name);
            publish_command_result(cmd.command_id, "failed", "unknown command");
        }
    }
}

static void handle_command_payload(const char *payload)
{
    cJSON *root;
    cJSON *command_id;
    cJSON *command_name;
    cJSON *payload_obj;
    cJSON *month;
    cJSON *force;
    ph_mqtt_command_t cmd = {0};

    root = cJSON_Parse(payload);
    if (root == NULL) {
        ESP_LOGW(TAG, "bad command json");
        return;
    }

    command_id = cJSON_GetObjectItem(root, "command_id");
    command_name = cJSON_GetObjectItem(root, "command_name");
    if (!cJSON_IsNumber(command_id) || !cJSON_IsString(command_name)) {
        ESP_LOGW(TAG, "bad command fields");
        cJSON_Delete(root);
        return;
    }

    cmd.command_id = command_id->valueint;
    snprintf(cmd.command_name, sizeof(cmd.command_name), "%s", command_name->valuestring);

    payload_obj = cJSON_GetObjectItem(root, "payload");
    if (cJSON_IsObject(payload_obj)) {
        month = cJSON_GetObjectItem(payload_obj, "month");
        if (cJSON_IsString(month)) {
            snprintf(cmd.month, sizeof(cmd.month), "%s", month->valuestring);
        }
        force = cJSON_GetObjectItem(payload_obj, "force");
        if (cJSON_IsBool(force)) {
            cmd.force_replay = cJSON_IsTrue(force);
        }
    }

    if (xQueueSend(s_command_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "command queue full");
    }

    cJSON_Delete(root);
}

static void publish_online_status(void)
{
    char topic[MQTT_TOPIC_MAX];

    build_topic("status", topic, sizeof(topic));
    esp_mqtt_client_publish(s_client, topic, "online", 0, 1, 1);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    (void)handler_args;
    (void)base;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED: {
            char topic[MQTT_TOPIC_MAX];
            xEventGroupSetBits(s_event_group, MQTT_CONNECTED_BIT);
            snprintf(s_status, sizeof(s_status), "CONNECTED");
            build_topic("commands/down", topic, sizeof(topic));
            esp_mqtt_client_subscribe(s_client, topic, 1);
            publish_online_status();
            request_upload_status_async();
            ESP_LOGI(TAG, "connected, subscribed %s", topic);
            break;
        }
        case MQTT_EVENT_DISCONNECTED:
            xEventGroupClearBits(s_event_group, MQTT_CONNECTED_BIT);
            snprintf(s_status, sizeof(s_status), "DISCONNECTED");
            ESP_LOGW(TAG, "disconnected");
            break;
        case MQTT_EVENT_PUBLISHED:
            if (event->msg_id == s_wait_msg_id) {
                xEventGroupSetBits(s_event_group, MQTT_PUBLISHED_BIT);
            }
            break;
        case MQTT_EVENT_DATA:
            if (event->current_data_offset != 0 ||
                event->data_len != event->total_data_len ||
                event->data_len >= MQTT_JSON_MAX) {
                ESP_LOGW(TAG, "unsupported fragmented command len=%d total=%d",
                         event->data_len, event->total_data_len);
                break;
            }
            {
                char *payload = (char *)malloc((size_t)event->data_len + 1U);

                if (payload == NULL) {
                    ESP_LOGW(TAG, "no mem for command payload");
                    break;
                }

                memcpy(payload, event->data, event->data_len);
                payload[event->data_len] = '\0';
                handle_command_payload(payload);
                free(payload);
            }
            break;
        case MQTT_EVENT_ERROR:
            snprintf(s_status, sizeof(s_status), "ERROR");
            snprintf(s_last_error, sizeof(s_last_error), "MQTT_ERROR");
            ESP_LOGW(TAG, "mqtt error");
            break;
        default:
            break;
    }
}

esp_err_t ph_mqtt_client_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = PH_MQTT_BROKER_URI,
        .credentials.username = PH_MQTT_USERNAME,
        .credentials.client_id = PH_MQTT_CLIENT_ID,
        .credentials.authentication.password = PH_MQTT_PASSWORD,
        .session.keepalive = 60,
        .session.last_will.qos = 1,
        .session.last_will.retain = 1,
        .network.reconnect_timeout_ms = 10000,
        .network.timeout_ms = 5000,
        .task.stack_size = MQTT_CLIENT_TASK_STACK,
    };
    static const char will_msg[] = "{\"status\":\"offline\"}";
    esp_err_t ret;

    if (s_client != NULL) {
        return ESP_OK;
    }
    if (PH_MQTT_BROKER_URI[0] == '\0') {
        snprintf(s_last_error, sizeof(s_last_error), "NO_MQTT_URI");
        return ESP_ERR_INVALID_STATE;
    }

    s_event_group = xEventGroupCreate();
    s_publish_mutex = xSemaphoreCreateMutex();
    s_upload_mutex = xSemaphoreCreateMutex();
    s_command_queue = xQueueCreate(MQTT_CMD_QUEUE_LEN, sizeof(ph_mqtt_command_t));
    if (s_event_group == NULL || s_publish_mutex == NULL ||
        s_upload_mutex == NULL || s_command_queue == NULL) {
        snprintf(s_last_error, sizeof(s_last_error), "NO_MEM");
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(upload_status_task, "ph_mqtt_status",
                    MQTT_STATUS_TASK_STACK, NULL, 4, &s_status_task) != pdPASS) {
        snprintf(s_last_error, sizeof(s_last_error), "STATUS_TASK_MEM");
        return ESP_ERR_NO_MEM;
    }

    build_topic("status", s_status_topic, sizeof(s_status_topic));
    mqtt_cfg.session.last_will.topic = s_status_topic;
    mqtt_cfg.session.last_will.msg = will_msg;
    mqtt_cfg.session.last_will.msg_len = 0;

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT_INIT");
        return ESP_FAIL;
    }

    ret = esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    if (ret != ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT_EVT:%s", esp_err_to_name(ret));
        return ret;
    }

    if (xTaskCreate(command_task, "ph_mqtt_cmd",
                    MQTT_COMMAND_TASK_STACK, NULL, 5, &s_command_task) != pdPASS) {
        snprintf(s_last_error, sizeof(s_last_error), "TASK_MEM");
        return ESP_ERR_NO_MEM;
    }

    ret = esp_mqtt_client_start(s_client);
    if (ret != ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "MQTT_START:%s", esp_err_to_name(ret));
        return ret;
    }

    snprintf(s_status, sizeof(s_status), "STARTED");
    return ESP_OK;
}
