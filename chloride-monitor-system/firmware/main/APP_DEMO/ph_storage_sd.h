#ifndef __PH_STORAGE_SD_H
#define __PH_STORAGE_SD_H

#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "ph_data_model.h"

#define PH_UPLOAD_CURSOR_FILE_MAX  32

typedef struct {
    char file[PH_UPLOAD_CURSOR_FILE_MAX];
    uint32_t line;
} ph_upload_cursor_t;

esp_err_t ph_storage_init(void);
bool ph_storage_is_ready(void);
esp_err_t ph_storage_append_sample(ph_sample_t *sample);
esp_err_t ph_storage_flush(void);
const char *ph_storage_get_last_error(void);
const char *ph_storage_get_current_path(void);
const char *ph_storage_get_backup_path(void);
esp_err_t ph_storage_create_test_csv(void);
esp_err_t ph_storage_create_test_csv_for_date(int year, int month, int day);
esp_err_t ph_storage_count_date(int year, int month, int day, uint32_t *out_count);
esp_err_t ph_upload_cursor_load(ph_upload_cursor_t *cursor);
esp_err_t ph_upload_cursor_save(const ph_upload_cursor_t *cursor);
esp_err_t ph_storage_mark_latest_sample_uploaded(int year, int month);
esp_err_t ph_storage_count_pending_upload(int year, int month, uint32_t *out_count);
esp_err_t ph_storage_get_pending_upload_summary(uint32_t *out_count,
                                                char *oldest_time,
                                                size_t oldest_time_size);
esp_err_t ph_storage_simulate_upload_month(int year, int month, uint32_t *out_uploaded_count);
esp_err_t ph_storage_delete_last_record(int year, int month, int day, uint32_t *out_deleted_count);
esp_err_t ph_storage_delete_date(int year, int month, int day, uint32_t *out_deleted_count);
esp_err_t ph_storage_delete_month(int year, int month);
esp_err_t ph_storage_delete_unknown(uint32_t *out_deleted_count);

#endif
