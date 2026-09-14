#ifndef __PH_SENSOR_RS485_H
#define __PH_SENSOR_RS485_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct
{
    float ph;
    float mv;
    float temperature;
    uint32_t ok_count;
    uint32_t err_count;
    uint8_t address;
    bool online;
} ph_sensor_data_t;

esp_err_t ph_sensor_init(void);
esp_err_t ph_sensor_read(ph_sensor_data_t *data);
const char *ph_sensor_get_last_error(void);

#endif
