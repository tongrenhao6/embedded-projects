#include "ph_data_model.h"

const char *ph_sensor_status_to_str(ph_sensor_status_t status)
{
    switch (status) {
        case PH_SENSOR_STATUS_OK:
            return "OK";
        case PH_SENSOR_STATUS_TIMEOUT:
            return "TMO";
        case PH_SENSOR_STATUS_SCAN:
            return "SCAN";
        case PH_SENSOR_STATUS_CRC:
            return "CRC";
        case PH_SENSOR_STATUS_FRAME:
            return "FRAME";
        case PH_SENSOR_STATUS_FLOAT:
            return "FLOAT";
        case PH_SENSOR_STATUS_TX:
            return "TX";
        case PH_SENSOR_STATUS_ARG:
            return "ARG";
        default:
            return "UNKNOWN";
    }
}

const char *ph_storage_status_to_str(ph_storage_status_t status)
{
    switch (status) {
        case PH_STORAGE_STATUS_PENDING:
            return "PENDING";
        case PH_STORAGE_STATUS_OK:
            return "OK";
        case PH_STORAGE_STATUS_SD_ERR:
            return "SD_ERR";
        case PH_STORAGE_STATUS_BACKUP_ERR:
            return "BACKUP_ERR";
        case PH_STORAGE_STATUS_WRITE_ERR:
            return "WRITE_ERR";
        default:
            return "UNKNOWN";
    }
}

const char *ph_upload_status_to_str(ph_upload_status_t status)
{
    switch (status) {
        case PH_UPLOAD_PENDING:
            return "PENDING";
        case PH_UPLOAD_SENT:
            return "SENT";
        case PH_UPLOAD_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}
