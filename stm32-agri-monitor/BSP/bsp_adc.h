/* bsp_adc.h - ADC1+DMA: PA0(light) PA1(soil), lookup table conversion */
#ifndef BSP_ADC_H
#define BSP_ADC_H

#include <stdint.h>

void     bsp_adc_init(void);                 /* start DMA continuous scan */
uint16_t bsp_adc_raw(int channel);           /* channel 0 or 1 */
uint16_t bsp_adc_get_lux(void);              /* light intensity (lux) */
uint8_t  bsp_adc_get_soil_grade(void);       /* soil moisture grade 1..4 */

#endif
