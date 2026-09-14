#include "ph_sampler.h"

#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "ph_data_model.h"
#include "ph_mqtt_client.h"
#include "ph_sensor_rs485.h"
#include "ph_storage_sd.h"
#include "ph_time.h"

#define PH_SAMPLER_TASK_STACK   (16384)
#define PH_SAMPLER_TASK_PRIO    4
#define PH_SAMPLE_SLOT_COUNT    6

static const char *TAG = "ph_sampler";
static const int s_sample_hours[PH_SAMPLE_SLOT_COUNT] = {0, 4, 8, 12, 16, 20};
static TaskHandle_t s_task_handle;
static SemaphoreHandle_t s_sample_lock;
static bool s_running;
static uint32_t s_sequence;
static int s_last_sample_key = -1;
static const char *s_status = "INIT";
static bool s_has_last_sample;
static ph_sample_t s_last_sample;

static esp_err_t ensure_sample_lock(void)
{
    if (s_sample_lock != NULL) {
        return ESP_OK;
    }

    s_sample_lock = xSemaphoreCreateMutex();
    if (s_sample_lock == NULL) {
        s_status = "NO_MEM";
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

static ph_sensor_status_t status_from_error(const char *error)
{
    if (error == NULL) {
        return PH_SENSOR_STATUS_UNKNOWN;
    }
    if (strcmp(error, "OK") == 0 || strcmp(error, "FOUND") == 0) {
        return PH_SENSOR_STATUS_OK;
    }
    if (strcmp(error, "TMO") == 0) {
        return PH_SENSOR_STATUS_TIMEOUT;
    }
    if (strcmp(error, "SCAN") == 0) {
        return PH_SENSOR_STATUS_SCAN;
    }
    if (strcmp(error, "CRC") == 0) {
        return PH_SENSOR_STATUS_CRC;
    }
    if (strcmp(error, "FRAME") == 0) {
        return PH_SENSOR_STATUS_FRAME;
    }
    if (strcmp(error, "FLOAT") == 0) {
        return PH_SENSOR_STATUS_FLOAT;
    }
    if (strcmp(error, "TX") == 0) {
        return PH_SENSOR_STATUS_TX;
    }
    if (strcmp(error, "ARG") == 0) {
        return PH_SENSOR_STATUS_ARG;
    }
    return PH_SENSOR_STATUS_UNKNOWN;
}

static int sample_slot_for_hour(int hour)
{
    for (int i = 0; i < PH_SAMPLE_SLOT_COUNT; i++) {
        if (hour == s_sample_hours[i]) {
            return i;
        }
    }

    return -1;
}

static int make_sample_key(const struct tm *timeinfo, int slot)
{
    return ((timeinfo->tm_year + 1900) * 10000 +
            (timeinfo->tm_mon + 1) * 100 +
            timeinfo->tm_mday) * 10 + slot;
}

static void fill_sample(ph_sample_t *sample, const ph_sensor_data_t *sensor_data, esp_err_t sensor_ret)
{
    memset(sample, 0, sizeof(*sample));

    sample->timestamp = ph_time_now();
    sample->uptime_ms = (uint64_t)(esp_timer_get_time() / 1000ULL);
    sample->time_valid = ph_time_is_valid();
    sample->sequence = sample->time_valid ? (uint32_t)sample->timestamp : ++s_sequence;
    s_sequence = sample->sequence;
    sample->sensor_type = PH_SENSOR_TYPE_PH;
    sample->sensor_status = status_from_error(ph_sensor_get_last_error());
    sample->storage_status = PH_STORAGE_STATUS_PENDING;
    sample->upload_status = PH_UPLOAD_PENDING;

    if (sensor_data != NULL) {
        sample->sensor_addr = sensor_data->address;
        sample->sensor_online = (sensor_ret == ESP_OK) && sensor_data->online;
        sample->ph = sensor_data->ph;
        sample->mv = sensor_data->mv;
        sample->temperature = sensor_data->temperature;
        sample->ok_count = sensor_data->ok_count;
        sample->error_count = sensor_data->err_count;
    }

    switch (sample->sensor_status) {
        case PH_SENSOR_STATUS_TIMEOUT:
        case PH_SENSOR_STATUS_SCAN:
            sample->timeout_count = sample->error_count;
            break;
        case PH_SENSOR_STATUS_CRC:
            sample->crc_error_count = sample->error_count;
            break;
        case PH_SENSOR_STATUS_FRAME:
            sample->frame_error_count = sample->error_count;
            break;
        case PH_SENSOR_STATUS_FLOAT:
            sample->float_error_count = sample->error_count;
            break;
        default:
            break;
    }
}

static esp_err_t take_and_store_sample(void)
{
    ph_sensor_data_t sensor_data = {0};
    ph_sample_t sample;
    esp_err_t sensor_ret;
    esp_err_t storage_ret;
    esp_err_t direct_ret = ESP_ERR_INVALID_STATE;
    bool direct_uploaded = false;
    int upload_year = 0;
    int upload_month = 0;

    if (ensure_sample_lock() != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    if (xSemaphoreTake(s_sample_lock, pdMS_TO_TICKS(1000)) != pdTRUE) {
        s_status = "BUSY";
        return ESP_ERR_TIMEOUT;
    }

    s_status = "SAMPLING";
    ESP_LOGI(TAG, "sampling start");
    ph_sensor_init();
    ESP_LOGI(TAG, "sensor read start");
    sensor_ret = ph_sensor_read(&sensor_data);
    ESP_LOGI(TAG, "sensor read done: %s", esp_err_to_name(sensor_ret));
    fill_sample(&sample, &sensor_data, sensor_ret);
    if (sensor_ret != ESP_OK) {
        s_status = "SENSOR_FAIL";
        ESP_LOGW(TAG, "sensor read failed: %s", ph_sensor_get_last_error());
        xSemaphoreGive(s_sample_lock);
        return sensor_ret;
    }

    if (sample.time_valid) {
        struct tm timeinfo;

        if (localtime_r(&sample.timestamp, &timeinfo) != NULL) {
            upload_year = timeinfo.tm_year + 1900;
            upload_month = timeinfo.tm_mon + 1;
        }
    }

    s_last_sample = sample;
    s_has_last_sample = true;

    if (ph_mqtt_client_is_connected()) {
        s_status = "MQTT_UPLOAD";
        ESP_LOGI(TAG, "sample sequence=%lu realtime upload start",
                 (unsigned long)sample.sequence);
        direct_ret = ph_mqtt_publish_sample_direct(&sample, "realtime");
        if (direct_ret == ESP_OK) {
            direct_uploaded = true;
            sample.upload_status = PH_UPLOAD_SENT;
            ESP_LOGI(TAG, "sample sequence=%lu realtime uploaded",
                     (unsigned long)sample.sequence);
        } else {
            sample.upload_status = PH_UPLOAD_FAILED;
            ESP_LOGW(TAG, "sample sequence=%lu realtime upload failed: %s",
                     (unsigned long)sample.sequence,
                     ph_mqtt_client_get_last_error());
        }
    } else {
        sample.upload_status = PH_UPLOAD_PENDING;
        ESP_LOGW(TAG, "sample sequence=%lu mqtt offline, keep pending: %s",
                 (unsigned long)sample.sequence,
                 ph_mqtt_client_get_status());
    }

    s_last_sample = sample;

    storage_ret = ph_storage_append_sample(&sample);
    if (storage_ret == ESP_OK) {
        s_status = (sample.storage_status == PH_STORAGE_STATUS_BACKUP_ERR) ?
                   "SAVED_BACKUP_WARN" : "SAVED";
        s_last_sample = sample;
        s_has_last_sample = true;
        if (sample.storage_status == PH_STORAGE_STATUS_BACKUP_ERR) {
            ESP_LOGW(TAG, "sample sequence=%lu saved, backup failed: %s",
                     (unsigned long)sample.sequence,
                     ph_storage_get_last_error());
        } else {
            ESP_LOGI(TAG, "sample sequence=%lu saved", (unsigned long)sample.sequence);
        }

        if (direct_uploaded && upload_year > 0 && upload_month > 0) {
            esp_err_t mark_ret = ph_storage_mark_latest_sample_uploaded(upload_year, upload_month);
            if (mark_ret == ESP_OK) {
                ESP_LOGI(TAG, "sample sequence=%lu marked uploaded in sd cursor",
                         (unsigned long)sample.sequence);
                ph_mqtt_publish_upload_status_current(NULL);
            } else {
                ESP_LOGW(TAG, "sample sequence=%lu saved but cursor not advanced: %s",
                         (unsigned long)sample.sequence,
                         ph_storage_get_last_error());
                ph_mqtt_publish_upload_status_current(ph_storage_get_last_error());
            }
        } else if (!direct_uploaded) {
            ph_mqtt_publish_upload_status_current(ph_mqtt_client_get_last_error());
        }
    } else {
        s_status = "SAVE_FAIL";
        ESP_LOGW(TAG, "save failed: %s", ph_storage_get_last_error());
        s_last_sample = sample;
        s_has_last_sample = true;

        if (direct_uploaded) {
            s_status = "UPLOADED_SD_FAIL";
            ESP_LOGW(TAG, "sample sequence=%lu already uploaded, sd save failed",
                     (unsigned long)sample.sequence);
            ph_mqtt_publish_upload_status_current(ph_storage_get_last_error());
        }
    }

    xSemaphoreGive(s_sample_lock);

    if (storage_ret == ESP_OK || direct_uploaded) {
        return ESP_OK;
    }

    return direct_ret != ESP_OK ? direct_ret : storage_ret;
}

static void ph_sampler_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "sampler task started");
    ph_sensor_init();
    s_status = "WAIT_TIME";

    while (1) {
        time_t now;
        struct tm timeinfo;
        int slot;

        if (!ph_time_is_valid()) {
            s_status = "WAIT_TIME";
            vTaskDelay(pdMS_TO_TICKS(10000));
            continue;
        }

        now = ph_time_now();
        localtime_r(&now, &timeinfo);
        slot = sample_slot_for_hour(timeinfo.tm_hour);
        if (slot >= 0 && timeinfo.tm_min < 10) {
            int sample_key = make_sample_key(&timeinfo, slot);
            if (sample_key != s_last_sample_key) {
                if (take_and_store_sample() == ESP_OK) {
                    s_last_sample_key = sample_key;
                }
            }
        } else {
            s_status = "IDLE";
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

esp_err_t ph_sampler_start(void)
{
    if (s_running) {
        return ESP_OK;
    }

    if (ensure_sample_lock() != ESP_OK) {
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "create sampler task");
    BaseType_t ret = xTaskCreatePinnedToCore(ph_sampler_task,
                                             "ph_sampler",
                                             PH_SAMPLER_TASK_STACK,
                                             NULL,
                                             PH_SAMPLER_TASK_PRIO,
                                             &s_task_handle,
                                             1);
    if (ret != pdPASS) {
        s_status = "NO_MEM";
        return ESP_ERR_NO_MEM;
    }

    s_running = true;
    s_status = "STARTED";
    return ESP_OK;
}

esp_err_t ph_sampler_take_now(void)
{
    if (!s_running) {
        esp_err_t ret = ph_sampler_start();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    return take_and_store_sample();
}

bool ph_sampler_is_running(void)
{
    return s_running;
}

uint32_t ph_sampler_get_sequence(void)
{
    return s_sequence;
}

const char *ph_sampler_get_status(void)
{
    return s_status;
}

bool ph_sampler_get_last_sample(ph_sample_t *sample)
{
    if (sample == NULL || !s_has_last_sample) {
        return false;
    }

    if (s_sample_lock != NULL && xSemaphoreTake(s_sample_lock, 0) != pdTRUE) {
        return false;
    }

    *sample = s_last_sample;

    if (s_sample_lock != NULL) {
        xSemaphoreGive(s_sample_lock);
    }

    return true;
}
