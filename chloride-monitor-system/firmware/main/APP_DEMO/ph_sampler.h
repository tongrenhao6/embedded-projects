#ifndef __PH_SAMPLER_H
#define __PH_SAMPLER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "ph_data_model.h"

esp_err_t ph_sampler_start(void);
esp_err_t ph_sampler_take_now(void);
bool ph_sampler_is_running(void);
uint32_t ph_sampler_get_sequence(void);
const char *ph_sampler_get_status(void);
bool ph_sampler_get_last_sample(ph_sample_t *sample);

#endif
