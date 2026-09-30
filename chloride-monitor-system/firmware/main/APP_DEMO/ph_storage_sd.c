#include "ph_storage_sd.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <time.h>
#include "esp_log.h"
#include "ff.h"
#include "ph_time.h"
#include "spi_sdcard.h"

#define PHLOG_ROOT_DIR          "0:/PHLOG"
#define PHLOG_DATA_DIR          "0:/PHLOG/data"
#define PHLOG_CURRENT_DIR       "0:/PHLOG/data/current"
#define PHLOG_BACKUP_DIR        "0:/PHLOG/data/backup"
#define PHLOG_INDEX_DIR         "0:/PHLOG/index"
#define PHLOG_TMP_DIR           "0:/PHLOG/tmp"
#define PHLOG_UPLOAD_CURSOR     "0:/PHLOG/index/upload.cursor"
#define CSV_LINE_MAX            384

static const char *TAG = "ph_storage";
static bool s_ready;
static char s_last_error[64] = "INIT";
static char s_current_path[64] = "";
static char s_backup_path[64] = "";

static esp_err_t count_data_lines_one_file(const char *path, uint32_t *count);

static esp_err_t fatfs_to_esp(FRESULT res)
{
    return res == FR_OK ? ESP_OK : ESP_FAIL;
}

static void set_fatfs_error(const char *op, FRESULT res)
{
    snprintf(s_last_error, sizeof(s_last_error), "%s:%d", op, (int)res);
}

static esp_err_t mkdir_if_needed(const char *path)
{
    FRESULT res = f_mkdir(path);

    if (res == FR_OK || res == FR_EXIST) {
        return ESP_OK;
    }

    set_fatfs_error("MKDIR", res);
    return ESP_FAIL;
}

static esp_err_t create_upload_cursor_if_needed(void)
{
    FIL fp;
    FRESULT res;
    UINT written;

    res = f_open(&fp, PHLOG_UPLOAD_CURSOR, FA_READ | FA_OPEN_EXISTING);
    if (res == FR_OK) {
        f_close(&fp);
        return ESP_OK;
    }

    res = f_open(&fp, PHLOG_UPLOAD_CURSOR, FA_WRITE | FA_CREATE_NEW);
    if (res == FR_EXIST) {
        return ESP_OK;
    }
    if (res != FR_OK) {
        set_fatfs_error("CURSOR", res);
        return ESP_FAIL;
    }

    const char *text = "file=\nsequence=0\nline=0\n";
    res = f_write(&fp, text, strlen(text), &written);
    if (res == FR_OK) {
        res = f_sync(&fp);
    }
    f_close(&fp);

    if (res != FR_OK || written != strlen(text)) {
        set_fatfs_error("CURSOR_WR", res);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void build_paths(time_t timestamp, bool time_valid)
{
    struct tm timeinfo;

    if (time_valid) {
        localtime_r(&timestamp, &timeinfo);
        snprintf(s_current_path, sizeof(s_current_path),
                 PHLOG_CURRENT_DIR "/ph_%04d_%02d.csv",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1);
        snprintf(s_backup_path, sizeof(s_backup_path),
                 PHLOG_BACKUP_DIR "/ph_%04d_%02d_bak.csv",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1);
    } else {
        snprintf(s_current_path, sizeof(s_current_path), PHLOG_CURRENT_DIR "/ph_unknown.csv");
        snprintf(s_backup_path, sizeof(s_backup_path), PHLOG_BACKUP_DIR "/ph_unknown_bak.csv");
    }
}

static void build_month_paths(int year, int month, char *current_path, size_t current_size,
                              char *backup_path, size_t backup_size)
{
    snprintf(current_path, current_size, PHLOG_CURRENT_DIR "/ph_%04d_%02d.csv", year, month);
    snprintf(backup_path, backup_size, PHLOG_BACKUP_DIR "/ph_%04d_%02d_bak.csv", year, month);
}

static void build_month_file_name(int year, int month, char *file_name, size_t file_name_size)
{
    snprintf(file_name, file_name_size, "ph_%04d_%02d.csv", year, month);
}

static void build_cursor_path(const char *file_name, char *cursor_path, size_t cursor_path_size)
{
    char stem[PH_UPLOAD_CURSOR_FILE_MAX];
    size_t len;

    if (cursor_path == NULL || cursor_path_size == 0) {
        return;
    }
    if (file_name == NULL || file_name[0] == '\0') {
        cursor_path[0] = '\0';
        return;
    }

    snprintf(stem, sizeof(stem), "%s", file_name);
    len = strlen(stem);
    if (len >= 4 && strcmp(stem + len - 4, ".csv") == 0) {
        stem[len - 4] = '\0';
    }
    snprintf(cursor_path, cursor_path_size, PHLOG_INDEX_DIR "/%s.cursor", stem);
}

static esp_err_t write_header_if_empty(FIL *fp)
{
    FRESULT res;
    UINT written;

    if (f_size(fp) != 0) {
        return ESP_OK;
    }

    const char *header =
        "date,collect_time,PH,temperature\n";

    res = f_write(fp, header, strlen(header), &written);
    if (res != FR_OK || written != strlen(header)) {
        set_fatfs_error("HEADER", res);
        return ESP_FAIL;
    }

    return ESP_OK;
}

static void format_date_time(time_t timestamp, bool time_valid,
                             char *date, size_t date_size,
                             char *time_text, size_t time_size)
{
    struct tm timeinfo;

    if (!time_valid) {
        snprintf(date, date_size, "unknown");
        snprintf(time_text, time_size, "unknown");
        return;
    }

    localtime_r(&timestamp, &timeinfo);
    snprintf(date, date_size, "%04d-%02d-%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday);
    snprintf(time_text, time_size, "%02d:%02d:%02d",
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
}

static esp_err_t append_sample_one_file(const char *path, const ph_sample_t *sample)
{
    FIL fp;
    FRESULT res;
    UINT written;
    char date[16];
    char time_text[16];
    char line[CSV_LINE_MAX];
    int len;

    res = f_open(&fp, path, FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) {
        set_fatfs_error("OPEN", res);
        return ESP_FAIL;
    }

    if (write_header_if_empty(&fp) != ESP_OK) {
        f_close(&fp);
        return ESP_FAIL;
    }

    res = f_lseek(&fp, f_size(&fp));
    if (res != FR_OK) {
        set_fatfs_error("SEEK", res);
        f_close(&fp);
        return ESP_FAIL;
    }

    format_date_time(sample->timestamp, sample->time_valid,
                     date, sizeof(date),
                     time_text, sizeof(time_text));
    len = snprintf(line, sizeof(line),
                   "%s,%s,%.2f,%.1f\n",
                   date,
                   time_text,
                   sample->ph,
                   sample->temperature);
    if (len <= 0 || len >= sizeof(line)) {
        snprintf(s_last_error, sizeof(s_last_error), "LINE_TOO_LONG");
        f_close(&fp);
        return ESP_FAIL;
    }

    res = f_write(&fp, line, len, &written);
    if (res == FR_OK) {
        res = f_sync(&fp);
    }
    f_close(&fp);

    if (res != FR_OK || written != len) {
        set_fatfs_error("WRITE", res);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ph_storage_init(void)
{
    esp_err_t ret;

    if (s_ready) {
        return ESP_OK;
    }

    ret = sd_spi_init();
    if (ret != ESP_OK) {
        snprintf(s_last_error, sizeof(s_last_error), "SD_INIT:%s", esp_err_to_name(ret));
        s_ready = false;
        return ret;
    }

    if (mkdir_if_needed(PHLOG_ROOT_DIR) != ESP_OK ||
        mkdir_if_needed(PHLOG_DATA_DIR) != ESP_OK ||
        mkdir_if_needed(PHLOG_CURRENT_DIR) != ESP_OK ||
        mkdir_if_needed(PHLOG_BACKUP_DIR) != ESP_OK ||
        mkdir_if_needed(PHLOG_INDEX_DIR) != ESP_OK ||
        mkdir_if_needed(PHLOG_TMP_DIR) != ESP_OK ||
        create_upload_cursor_if_needed() != ESP_OK) {
        s_ready = false;
        return ESP_FAIL;
    }

    s_ready = true;
    snprintf(s_last_error, sizeof(s_last_error), "OK");
    ESP_LOGI(TAG, "storage ready");
    return ESP_OK;
}

bool ph_storage_is_ready(void)
{
    return s_ready;
}

esp_err_t ph_storage_append_sample(ph_sample_t *sample)
{
    esp_err_t ret;
    esp_err_t backup_ret;

    if (sample == NULL) {
        snprintf(s_last_error, sizeof(s_last_error), "ARG");
        return ESP_ERR_INVALID_ARG;
    }

    ret = ph_storage_init();
    if (ret != ESP_OK) {
        sample->storage_status = PH_STORAGE_STATUS_SD_ERR;
        return ret;
    }

    sample->storage_status = PH_STORAGE_STATUS_OK;
    build_paths(sample->timestamp, sample->time_valid);

    ret = append_sample_one_file(s_current_path, sample);
    if (ret != ESP_OK) {
        sample->storage_status = PH_STORAGE_STATUS_WRITE_ERR;
        return ret;
    }

    backup_ret = append_sample_one_file(s_backup_path, sample);
    if (backup_ret != ESP_OK) {
        sample->storage_status = PH_STORAGE_STATUS_BACKUP_ERR;
        snprintf(s_last_error, sizeof(s_last_error), "BACKUP_ERR");
        ESP_LOGW(TAG, "sample saved to current, backup failed: %s", s_backup_path);
        return ESP_OK;
    }

    sample->storage_status = PH_STORAGE_STATUS_OK;
    snprintf(s_last_error, sizeof(s_last_error), "OK");
    ESP_LOGI(TAG, "sample saved: %s", s_current_path);
    return ESP_OK;
}

esp_err_t ph_storage_flush(void)
{
    return s_ready ? ESP_OK : ESP_ERR_INVALID_STATE;
}

const char *ph_storage_get_last_error(void)
{
    return s_last_error;
}

const char *ph_storage_get_current_path(void)
{
    return s_current_path;
}

const char *ph_storage_get_backup_path(void)
{
    return s_backup_path;
}

esp_err_t ph_upload_cursor_load(ph_upload_cursor_t *cursor)
{
    FIL fp;
    FRESULT res;
    UINT read_len;
    char text[96];
    char *line;
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    char cursor_path[96];

    if (cursor == NULL || cursor->file[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    snprintf(file_name, sizeof(file_name), "%s", cursor->file);
    cursor->line = 0;

    build_cursor_path(file_name, cursor_path, sizeof(cursor_path));
    res = f_open(&fp, cursor_path, FA_READ | FA_OPEN_EXISTING);
    if (res == FR_NO_FILE) {
        /* Fallback to the legacy shared cursor file to preserve existing progress. */
        res = f_open(&fp, PHLOG_UPLOAD_CURSOR, FA_READ | FA_OPEN_EXISTING);
        if (res == FR_NO_FILE) {
            snprintf(cursor->file, sizeof(cursor->file), "%s", file_name);
            return ESP_OK;
        }
    }
    if (res != FR_OK) {
        set_fatfs_error("CURSOR_RD", res);
        return ESP_FAIL;
    }

    res = f_read(&fp, text, sizeof(text) - 1, &read_len);
    f_close(&fp);
    if (res != FR_OK) {
        set_fatfs_error("CURSOR_RD", res);
        return ESP_FAIL;
    }

    text[read_len] = '\0';
    line = strtok(text, "\r\n");
    while (line != NULL) {
        if (strncmp(line, "file=", 5) == 0) {
            snprintf(cursor->file, sizeof(cursor->file), "%s", line + 5);
        } else if (strncmp(line, "line=", 5) == 0) {
            cursor->line = (uint32_t)strtoul(line + 5, NULL, 10);
        } else {
            cursor->line = (uint32_t)strtoul(line, NULL, 10);
        }
        line = strtok(NULL, "\r\n");
    }

    if (cursor->file[0] == '\0') {
        snprintf(cursor->file, sizeof(cursor->file), "%s", file_name);
    }
    if (strcmp(cursor->file, file_name) != 0) {
        snprintf(cursor->file, sizeof(cursor->file), "%s", file_name);
        cursor->line = 0;
    }

    return ESP_OK;
}

esp_err_t ph_upload_cursor_save(const ph_upload_cursor_t *cursor)
{
    FIL fp;
    FRESULT res;
    UINT written;
    char text[96];
    char cursor_path[96];
    int len;

    if (cursor == NULL || cursor->file[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    len = snprintf(text, sizeof(text), "line=%lu\n", (unsigned long)cursor->line);
    if (len <= 0 || len >= sizeof(text)) {
        snprintf(s_last_error, sizeof(s_last_error), "CURSOR_LONG");
        return ESP_FAIL;
    }

    build_cursor_path(cursor->file, cursor_path, sizeof(cursor_path));
    res = f_open(&fp, cursor_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        set_fatfs_error("CURSOR_WR", res);
        return ESP_FAIL;
    }

    res = f_write(&fp, text, len, &written);
    if (res == FR_OK) {
        res = f_sync(&fp);
    }
    f_close(&fp);

    if (res != FR_OK || written != len) {
        set_fatfs_error("CURSOR_WR", res);
        return ESP_FAIL;
    }

    snprintf(s_last_error, sizeof(s_last_error), "OK");
    return ESP_OK;
}

esp_err_t ph_storage_mark_latest_sample_uploaded(int year, int month)
{
    char current_path[64];
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    ph_upload_cursor_t cursor;
    uint32_t total_count = 0;

    if (year < 2024 || month < 1 || month > 12) {
        snprintf(s_last_error, sizeof(s_last_error), "BAD_MONTH");
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    build_month_file_name(year, month, file_name, sizeof(file_name));
    snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/%s", file_name);
    if (count_data_lines_one_file(current_path, &total_count) != ESP_OK) {
        return ESP_FAIL;
    }
    if (total_count == 0) {
        snprintf(s_last_error, sizeof(s_last_error), "NO_DATA");
        return ESP_ERR_NOT_FOUND;
    }

    memset(&cursor, 0, sizeof(cursor));
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
    if (ph_upload_cursor_load(&cursor) != ESP_OK) {
        return ESP_FAIL;
    }

    if (cursor.line >= total_count) {
        snprintf(s_last_error, sizeof(s_last_error), "OK");
        return ESP_OK;
    }
    if (cursor.line + 1U != total_count) {
        snprintf(s_last_error, sizeof(s_last_error), "BACKLOG:%lu/%lu",
                 (unsigned long)cursor.line,
                 (unsigned long)total_count);
        return ESP_ERR_INVALID_STATE;
    }

    cursor.line = total_count;
    return ph_upload_cursor_save(&cursor);
}

esp_err_t ph_storage_create_test_csv(void)
{
    time_t now;
    struct tm timeinfo;

    now = ph_time_now();
    if (!ph_time_is_valid()) {
        snprintf(s_last_error, sizeof(s_last_error), "TIME_INVALID");
        return ESP_ERR_INVALID_STATE;
    }

    localtime_r(&now, &timeinfo);
    return ph_storage_create_test_csv_for_date(timeinfo.tm_year + 1900,
                                               timeinfo.tm_mon + 1,
                                               timeinfo.tm_mday);
}

esp_err_t ph_storage_create_test_csv_for_date(int year, int month, int day)
{
    struct tm timeinfo = {0};

    if (year < 2024 || month < 1 || month > 12 || day < 1 || day > 31) {
        snprintf(s_last_error, sizeof(s_last_error), "BAD_DATE");
        return ESP_ERR_INVALID_ARG;
    }

    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    timeinfo.tm_year = year - 1900;
    timeinfo.tm_mon = month - 1;
    timeinfo.tm_mday = day;
    timeinfo.tm_isdst = -1;

    for (int i = 0; i < 6; i++) {
        ph_sample_t sample = {0};

        timeinfo.tm_hour = i * 4;
        timeinfo.tm_min = 0;
        timeinfo.tm_sec = 0;

        sample.timestamp = mktime(&timeinfo);
        sample.uptime_ms = 0;
        sample.time_valid = true;
        sample.sequence = (uint32_t)sample.timestamp;
        sample.sensor_type = PH_SENSOR_TYPE_PH;
        sample.sensor_addr = 3;
        sample.sensor_online = true;
        sample.sensor_status = PH_SENSOR_STATUS_OK;
        sample.ph = 7.80f + (float)i * 0.10f;
        sample.mv = -45.0f - (float)i * 2.5f;
        sample.temperature = 25.0f + (float)i * 0.2f;
        sample.ok_count = i + 1;
        sample.storage_status = PH_STORAGE_STATUS_PENDING;
        sample.upload_status = PH_UPLOAD_PENDING;

        if (ph_storage_append_sample(&sample) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    snprintf(s_last_error, sizeof(s_last_error), "TEST_CSV_OK");
    return ESP_OK;
}

static bool line_matches_date(const char *line, int year, int month, int day)
{
    char date_text[16];

    snprintf(date_text, sizeof(date_text), "%04d-%02d-%02d", year, month, day);
    return strstr(line, date_text) != NULL;
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

static bool parse_sample_line_fields(char *line, char *date, size_t date_size,
                                     char *time_text, size_t time_size)
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
    return true;
}

static bool sample_time_text_is_valid(const char *date, const char *time_text)
{
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;

    if (date == NULL || time_text == NULL) {
        return false;
    }

    return sscanf(date, "%4d-%2d-%2d", &year, &month, &day) == 3 &&
           sscanf(time_text, "%2d:%2d:%2d", &hour, &minute, &second) == 3;
}

static bool is_pending_csv_file_name(const char *file_name)
{
    size_t len;

    if (file_name == NULL) {
        return false;
    }

    len = strlen(file_name);
    return len >= 4 && strcmp(file_name + len - 4, ".csv") == 0;
}

static esp_err_t count_data_lines_one_file(const char *path, uint32_t *count)
{
    FIL *fp;
    FRESULT res;
    char *line;

    *count = 0;
    fp = (FIL *)malloc(sizeof(FIL));
    line = (char *)malloc(CSV_LINE_MAX);
    if (fp == NULL || line == NULL) {
        free(fp);
        free(line);
        snprintf(s_last_error, sizeof(s_last_error), "NO_MEM");
        return ESP_ERR_NO_MEM;
    }

    res = f_open(fp, path, FA_READ | FA_OPEN_EXISTING);
    if (res == FR_NO_FILE) {
        free(fp);
        free(line);
        return ESP_OK;
    }
    if (res != FR_OK) {
        set_fatfs_error("UPLOAD_OPEN", res);
        free(fp);
        free(line);
        return ESP_FAIL;
    }

    while (read_csv_line(fp, line, CSV_LINE_MAX)) {
        if (is_data_line(line)) {
            (*count)++;
        }
    }

    f_close(fp);
    free(fp);
    free(line);
    return ESP_OK;
}

static esp_err_t get_uploaded_line_for_file(const char *file_name, uint32_t *uploaded_line)
{
    ph_upload_cursor_t cursor;

    if (uploaded_line == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *uploaded_line = 0;
    memset(&cursor, 0, sizeof(cursor));
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
    if (ph_upload_cursor_load(&cursor) != ESP_OK) {
        return ESP_OK;
    }
    *uploaded_line = cursor.line;

    return ESP_OK;
}

static esp_err_t find_oldest_pending_time_in_file(const char *path,
                                                  uint32_t uploaded_line,
                                                  char *oldest_time,
                                                  size_t oldest_time_size)
{
    FIL *fp;
    FRESULT res;
    char *line;
    uint32_t data_index = 0;

    if (oldest_time == NULL || oldest_time_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    oldest_time[0] = '\0';
    fp = (FIL *)malloc(sizeof(FIL));
    line = (char *)malloc(CSV_LINE_MAX);
    if (fp == NULL || line == NULL) {
        free(fp);
        free(line);
        snprintf(s_last_error, sizeof(s_last_error), "NO_MEM");
        return ESP_ERR_NO_MEM;
    }

    res = f_open(fp, path, FA_READ | FA_OPEN_EXISTING);
    if (res == FR_NO_FILE) {
        free(fp);
        free(line);
        return ESP_OK;
    }
    if (res != FR_OK) {
        set_fatfs_error("UPLOAD_OPEN", res);
        free(fp);
        free(line);
        return ESP_FAIL;
    }

    while (read_csv_line(fp, line, CSV_LINE_MAX)) {
        char date[16];
        char time_text[16];

        if (!is_data_line(line)) {
            continue;
        }

        data_index++;
        if (data_index <= uploaded_line) {
            continue;
        }

        if (parse_sample_line_fields(line, date, sizeof(date), time_text, sizeof(time_text)) &&
            sample_time_text_is_valid(date, time_text)) {
            snprintf(oldest_time, oldest_time_size, "%s %s", date, time_text);
        }
        break;
    }

    f_close(fp);
    free(fp);
    free(line);
    return ESP_OK;
}

static void clamp_upload_cursor_for_month(int year, int month)
{
    char current_path[64];
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    ph_upload_cursor_t cursor;
    uint32_t total_count = 0;

    build_month_file_name(year, month, file_name, sizeof(file_name));
    memset(&cursor, 0, sizeof(cursor));
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
    if (ph_upload_cursor_load(&cursor) != ESP_OK) {
        return;
    }

    snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/%s", file_name);
    if (count_data_lines_one_file(current_path, &total_count) != ESP_OK) {
        return;
    }

    if (cursor.line > total_count) {
        cursor.line = total_count;
        ph_upload_cursor_save(&cursor);
    }
}

esp_err_t ph_storage_count_pending_upload(int year, int month, uint32_t *out_count)
{
    char current_path[64];
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    ph_upload_cursor_t cursor;
    uint32_t total_count = 0;
    uint32_t uploaded_count = 0;

    if (out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (year < 2024 || month < 1 || month > 12) {
        snprintf(s_last_error, sizeof(s_last_error), "BAD_MONTH");
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    build_month_file_name(year, month, file_name, sizeof(file_name));
    snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/%s", file_name);
    if (count_data_lines_one_file(current_path, &total_count) != ESP_OK) {
        return ESP_FAIL;
    }

    memset(&cursor, 0, sizeof(cursor));
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
    if (ph_upload_cursor_load(&cursor) != ESP_OK) {
        return ESP_FAIL;
    }
    uploaded_count = cursor.line;
    if (uploaded_count > total_count) {
        uploaded_count = total_count;
    }

    *out_count = total_count - uploaded_count;
    snprintf(s_last_error, sizeof(s_last_error), "OK");
    return ESP_OK;
}

esp_err_t ph_storage_get_pending_upload_summary(uint32_t *out_count,
                                                char *oldest_time,
                                                size_t oldest_time_size)
{
    FF_DIR dir;
    FILINFO file_info;
    FRESULT res;
    uint32_t total_pending = 0;
    char best_oldest[20] = "";

    if (out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (oldest_time != NULL && oldest_time_size > 0) {
        oldest_time[0] = '\0';
    }
    *out_count = 0;

    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    res = f_opendir(&dir, PHLOG_CURRENT_DIR);
    if (res != FR_OK) {
        set_fatfs_error("DIR_OPEN", res);
        return ESP_FAIL;
    }

    while (1) {
        char current_path[320];
        uint32_t total_count = 0;
        uint32_t uploaded_count = 0;
        uint32_t pending_count = 0;
        char file_oldest[20] = "";

        res = f_readdir(&dir, &file_info);
        if (res != FR_OK) {
            set_fatfs_error("DIR_READ", res);
            f_closedir(&dir);
            return ESP_FAIL;
        }
        if (file_info.fname[0] == '\0') {
            break;
        }
        if ((file_info.fattrib & AM_DIR) != 0 || !is_pending_csv_file_name(file_info.fname)) {
            continue;
        }

        snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/%s", file_info.fname);
        if (count_data_lines_one_file(current_path, &total_count) != ESP_OK) {
            f_closedir(&dir);
            return ESP_FAIL;
        }
        if (get_uploaded_line_for_file(file_info.fname, &uploaded_count) != ESP_OK) {
            f_closedir(&dir);
            return ESP_FAIL;
        }
        if (uploaded_count > total_count) {
            uploaded_count = total_count;
        }

        pending_count = total_count - uploaded_count;
        total_pending += pending_count;
        if (pending_count == 0) {
            continue;
        }

        if (find_oldest_pending_time_in_file(current_path, uploaded_count,
                                             file_oldest, sizeof(file_oldest)) != ESP_OK) {
            f_closedir(&dir);
            return ESP_FAIL;
        }
        if (file_oldest[0] != '\0' &&
            (best_oldest[0] == '\0' || strcmp(file_oldest, best_oldest) < 0)) {
            snprintf(best_oldest, sizeof(best_oldest), "%s", file_oldest);
        }
    }

    f_closedir(&dir);
    *out_count = total_pending;
    if (oldest_time != NULL && oldest_time_size > 0 && best_oldest[0] != '\0') {
        snprintf(oldest_time, oldest_time_size, "%s", best_oldest);
    }
    snprintf(s_last_error, sizeof(s_last_error), "OK");
    return ESP_OK;
}

esp_err_t ph_storage_simulate_upload_month(int year, int month, uint32_t *out_uploaded_count)
{
    char current_path[64];
    char file_name[PH_UPLOAD_CURSOR_FILE_MAX];
    ph_upload_cursor_t cursor;
    uint32_t total_count = 0;
    uint32_t pending_count = 0;

    if (out_uploaded_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    build_month_file_name(year, month, file_name, sizeof(file_name));
    snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/%s", file_name);
    if (count_data_lines_one_file(current_path, &total_count) != ESP_OK) {
        return ESP_FAIL;
    }
    if (ph_storage_count_pending_upload(year, month, &pending_count) != ESP_OK) {
        return ESP_FAIL;
    }

    memset(&cursor, 0, sizeof(cursor));
    snprintf(cursor.file, sizeof(cursor.file), "%s", file_name);
    cursor.line = total_count;
    if (ph_upload_cursor_save(&cursor) != ESP_OK) {
        return ESP_FAIL;
    }

    *out_uploaded_count = pending_count;
    snprintf(s_last_error, sizeof(s_last_error), "UPLOAD_SIM:%lu", (unsigned long)pending_count);
    return ESP_OK;
}

static esp_err_t count_date_one_file(const char *path, int year, int month, int day, uint32_t *count)
{
    FIL *fp;
    FRESULT res;
    char *line;

    *count = 0;
    fp = (FIL *)malloc(sizeof(FIL));
    line = (char *)malloc(CSV_LINE_MAX);
    if (fp == NULL || line == NULL) {
        free(fp);
        free(line);
        snprintf(s_last_error, sizeof(s_last_error), "NO_MEM");
        return ESP_ERR_NO_MEM;
    }

    res = f_open(fp, path, FA_READ | FA_OPEN_EXISTING);
    if (res == FR_NO_FILE) {
        free(fp);
        free(line);
        return ESP_OK;
    }
    if (res != FR_OK) {
        set_fatfs_error("SEARCH_OPEN", res);
        free(fp);
        free(line);
        return ESP_FAIL;
    }

    while (read_csv_line(fp, line, CSV_LINE_MAX)) {
        if (line_matches_date(line, year, month, day)) {
            (*count)++;
        }
    }

    f_close(fp);
    free(fp);
    free(line);
    return ESP_OK;
}

esp_err_t ph_storage_count_date(int year, int month, int day, uint32_t *out_count)
{
    char current_path[64];

    if (out_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    snprintf(current_path, sizeof(current_path), PHLOG_CURRENT_DIR "/ph_%04d_%02d.csv", year, month);
    return count_date_one_file(current_path, year, month, day, out_count);
}

static esp_err_t delete_date_one_file(const char *path, int year, int month, int day,
                                      bool latest_only, uint32_t *deleted_count)
{
    FIL *src;
    FIL *dst;
    FRESULT res;
    UINT written;
    char tmp_path[80];
    char *line;
    uint32_t match_count = 0;
    uint32_t match_index = 0;
    uint32_t target_index = 0;

    *deleted_count = 0;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    if (latest_only) {
        esp_err_t ret = count_date_one_file(path, year, month, day, &match_count);
        if (ret != ESP_OK || match_count == 0) {
            return ret;
        }
        target_index = match_count;
    }

    src = (FIL *)malloc(sizeof(FIL));
    dst = (FIL *)malloc(sizeof(FIL));
    line = (char *)malloc(CSV_LINE_MAX);
    if (src == NULL || dst == NULL || line == NULL) {
        free(src);
        free(dst);
        free(line);
        snprintf(s_last_error, sizeof(s_last_error), "NO_MEM");
        return ESP_ERR_NO_MEM;
    }

    res = f_open(src, path, FA_READ | FA_OPEN_EXISTING);
    if (res == FR_NO_FILE) {
        free(src);
        free(dst);
        free(line);
        return ESP_OK;
    }
    if (res != FR_OK) {
        set_fatfs_error("DEL_OPEN", res);
        free(src);
        free(dst);
        free(line);
        return ESP_FAIL;
    }

    f_unlink(tmp_path);
    res = f_open(dst, tmp_path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        set_fatfs_error("TMP_OPEN", res);
        f_close(src);
        free(src);
        free(dst);
        free(line);
        return ESP_FAIL;
    }

    while (read_csv_line(src, line, CSV_LINE_MAX)) {
        if (line_matches_date(line, year, month, day)) {
            match_index++;
            if (!latest_only || match_index == target_index) {
                (*deleted_count)++;
                continue;
            }
        }

        res = f_write(dst, line, strlen(line), &written);
        if (res != FR_OK || written != strlen(line)) {
            set_fatfs_error("TMP_WRITE", res);
            f_close(src);
            f_close(dst);
            free(src);
            free(dst);
            free(line);
            f_unlink(tmp_path);
            return ESP_FAIL;
        }
    }

    res = f_sync(dst);
    f_close(src);
    f_close(dst);
    free(src);
    free(dst);
    free(line);
    if (res != FR_OK) {
        set_fatfs_error("TMP_SYNC", res);
        f_unlink(tmp_path);
        return ESP_FAIL;
    }

    res = f_unlink(path);
    if (res != FR_OK && res != FR_NO_FILE) {
        set_fatfs_error("DEL_OLD", res);
        f_unlink(tmp_path);
        return ESP_FAIL;
    }

    res = f_rename(tmp_path, path);
    if (res != FR_OK) {
        set_fatfs_error("RENAME", res);
        f_unlink(tmp_path);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t ph_storage_delete_last_record(int year, int month, int day, uint32_t *out_deleted_count)
{
    char current_path[64];
    char backup_path[64];
    uint32_t current_deleted = 0;
    uint32_t backup_deleted = 0;

    if (out_deleted_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    build_month_paths(year, month, current_path, sizeof(current_path), backup_path, sizeof(backup_path));
    if (delete_date_one_file(current_path, year, month, day, true, &current_deleted) != ESP_OK) {
        return ESP_FAIL;
    }
    if (delete_date_one_file(backup_path, year, month, day, true, &backup_deleted) != ESP_OK) {
        return ESP_FAIL;
    }

    *out_deleted_count = current_deleted;
    snprintf(s_last_error, sizeof(s_last_error), "DEL_ONE:%lu/%lu",
             (unsigned long)current_deleted,
             (unsigned long)backup_deleted);
    clamp_upload_cursor_for_month(year, month);
    return ESP_OK;
}

esp_err_t ph_storage_delete_date(int year, int month, int day, uint32_t *out_deleted_count)
{
    char current_path[64];
    char backup_path[64];
    uint32_t current_deleted = 0;
    uint32_t backup_deleted = 0;

    if (out_deleted_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    build_month_paths(year, month, current_path, sizeof(current_path), backup_path, sizeof(backup_path));
    if (delete_date_one_file(current_path, year, month, day, false, &current_deleted) != ESP_OK) {
        return ESP_FAIL;
    }
    if (delete_date_one_file(backup_path, year, month, day, false, &backup_deleted) != ESP_OK) {
        return ESP_FAIL;
    }

    *out_deleted_count = current_deleted;
    snprintf(s_last_error, sizeof(s_last_error), "DEL:%lu/%lu",
             (unsigned long)current_deleted,
             (unsigned long)backup_deleted);
    clamp_upload_cursor_for_month(year, month);
    return ESP_OK;
}

esp_err_t ph_storage_delete_month(int year, int month)
{
    char current_path[64];
    char backup_path[64];
    FRESULT current_res;
    FRESULT backup_res;

    if (year < 2024 || month < 1 || month > 12) {
        snprintf(s_last_error, sizeof(s_last_error), "BAD_MONTH");
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    build_month_paths(year, month, current_path, sizeof(current_path), backup_path, sizeof(backup_path));

    current_res = f_unlink(current_path);
    backup_res = f_unlink(backup_path);
    if ((current_res == FR_OK || current_res == FR_NO_FILE) &&
        (backup_res == FR_OK || backup_res == FR_NO_FILE)) {
        snprintf(s_last_error, sizeof(s_last_error), "OK");
        clamp_upload_cursor_for_month(year, month);
        return ESP_OK;
    }

    set_fatfs_error("DEL", current_res != FR_OK ? current_res : backup_res);
    return fatfs_to_esp(current_res != FR_OK ? current_res : backup_res);
}

esp_err_t ph_storage_delete_unknown(uint32_t *out_deleted_count)
{
    const char *current_path = PHLOG_CURRENT_DIR "/ph_unknown.csv";
    const char *backup_path = PHLOG_BACKUP_DIR "/ph_unknown_bak.csv";
    char cursor_path[96];
    FRESULT current_res;
    FRESULT backup_res;
    FRESULT cursor_res;
    uint32_t current_count = 0;

    if (out_deleted_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (ph_storage_init() != ESP_OK) {
        return ESP_FAIL;
    }

    if (count_data_lines_one_file(current_path, &current_count) != ESP_OK) {
        return ESP_FAIL;
    }

    build_cursor_path("ph_unknown.csv", cursor_path, sizeof(cursor_path));

    current_res = f_unlink(current_path);
    backup_res = f_unlink(backup_path);
    cursor_res = f_unlink(cursor_path);

    if ((current_res == FR_OK || current_res == FR_NO_FILE) &&
        (backup_res == FR_OK || backup_res == FR_NO_FILE) &&
        (cursor_res == FR_OK || cursor_res == FR_NO_FILE)) {
        *out_deleted_count = current_count;
        snprintf(s_last_error, sizeof(s_last_error), "DEL_UNKNOWN:%lu",
                 (unsigned long)current_count);
        return ESP_OK;
    }

    set_fatfs_error("DEL_UNKNOWN",
                    current_res != FR_OK && current_res != FR_NO_FILE ? current_res :
                    backup_res != FR_OK && backup_res != FR_NO_FILE ? backup_res :
                    cursor_res);
    return ESP_FAIL;
}
