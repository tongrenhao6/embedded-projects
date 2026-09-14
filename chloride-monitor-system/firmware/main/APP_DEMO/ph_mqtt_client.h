#ifndef __PH_MQTT_CLIENT_H
#define __PH_MQTT_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "ph_data_model.h"

esp_err_t ph_mqtt_client_start(void);
bool ph_mqtt_client_is_connected(void);
const char *ph_mqtt_client_get_status(void);
const char *ph_mqtt_client_get_last_error(void);
esp_err_t ph_mqtt_upload_month(int year, int month, uint32_t *out_uploaded_count);
esp_err_t ph_mqtt_publish_sample_direct(const ph_sample_t *sample, const char *source);
esp_err_t ph_mqtt_publish_upload_status_current(const char *last_error);

#endif
