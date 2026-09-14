#ifndef __PH_DATA_MODEL_H
#define __PH_DATA_MODEL_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    PH_SENSOR_TYPE_PH = 1,
    PH_SENSOR_TYPE_ORP,
    PH_SENSOR_TYPE_EC,
    PH_SENSOR_TYPE_TURBIDITY,
    PH_SENSOR_TYPE_DO,
    PH_SENSOR_TYPE_LEVEL,
} ph_sensor_type_t;

typedef enum {
    PH_SENSOR_STATUS_OK = 0,
    PH_SENSOR_STATUS_TIMEOUT,
    PH_SENSOR_STATUS_SCAN,
    PH_SENSOR_STATUS_CRC,
    PH_SENSOR_STATUS_FRAME,
    PH_SENSOR_STATUS_FLOAT,
    PH_SENSOR_STATUS_TX,
    PH_SENSOR_STATUS_ARG,
    PH_SENSOR_STATUS_UNKNOWN,
} ph_sensor_status_t;

typedef enum {
    PH_STORAGE_STATUS_PENDING = 0,
    PH_STORAGE_STATUS_OK,
    PH_STORAGE_STATUS_SD_ERR,
    PH_STORAGE_STATUS_BACKUP_ERR,
    PH_STORAGE_STATUS_WRITE_ERR,
} ph_storage_status_t;

typedef enum {
    PH_UPLOAD_PENDING = 0,
    PH_UPLOAD_SENT,
    PH_UPLOAD_FAILED,
} ph_upload_status_t;

typedef struct {
    time_t timestamp;
    uint64_t uptime_ms;
    bool time_valid;
    uint32_t sequence;

    ph_sensor_type_t sensor_type;
    uint8_t sensor_addr;
    bool sensor_online;
    ph_sensor_status_t sensor_status;

    float ph;
    float mv;
    float temperature;

    uint32_t ok_count;
    uint32_t timeout_count;
    uint32_t crc_error_count;
    uint32_t frame_error_count;
    uint32_t float_error_count;
    uint32_t error_count;

    ph_storage_status_t storage_status;
    ph_upload_status_t upload_status;
} ph_sample_t;

const char *ph_sensor_status_to_str(ph_sensor_status_t status);
const char *ph_storage_status_to_str(ph_storage_status_t status);
const char *ph_upload_status_to_str(ph_upload_status_t status);

#endif
