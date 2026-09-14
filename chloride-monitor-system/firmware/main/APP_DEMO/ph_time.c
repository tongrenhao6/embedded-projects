/*
    该文件用于实现时间同步功能，
    包括初始化SNTP、检查时间有效性和获取当前时间。
*/

#include "ph_time.h"

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "ph_config.h"

static const char *TAG = "ph_time";
static bool s_time_inited;
static bool s_time_valid;
static const char *s_time_status = "INIT";

static void ph_time_sync_cb(struct timeval *tv)
{
    time_t now = tv->tv_sec;
    struct tm timeinfo;

    localtime_r(&now, &timeinfo);
    s_time_valid = (timeinfo.tm_year + 1900) >= 2024;
    s_time_status = s_time_valid ? "SYNCED" : "BAD_TIME";

    ESP_LOGI(TAG, "time sync: %04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

esp_err_t ph_time_init(void)
{
    if (s_time_inited)
    {
        return ESP_OK;
    }

    setenv("TZ", "CST-8", 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, PH_SNTP_SERVER_1);
    esp_sntp_set_time_sync_notification_cb(ph_time_sync_cb);
    esp_sntp_init();

    s_time_inited = true;
    s_time_status = "WAIT_SYNC";
    ESP_LOGI(TAG, "sntp started");
    return ESP_OK;
}

bool ph_time_is_valid(void)
{
    time_t now;
    struct tm timeinfo;

    if (s_time_valid)
    {
        return true;
    }

    time(&now);
    localtime_r(&now, &timeinfo); 
    s_time_valid = (timeinfo.tm_year + 1900) >= 2024;
    if (s_time_valid)
    {
        s_time_status = "VALID";
    }

    return s_time_valid;
}

time_t ph_time_now(void)
{
    time_t now;

    time(&now);
    return now;
}

const char *ph_time_get_status(void)
{
    return s_time_status;
}
