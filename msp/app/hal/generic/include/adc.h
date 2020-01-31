/*
 * ADC driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef HAL_GENERIC_INCLUDE_ADC_H_
#define HAL_GENERIC_INCLUDE_ADC_H_

#include <hal.h>

void adc_init(void);
uint16_t adc_conversion(unsigned channel);
void adc_deinit(void);

#endif /* HAL_GENERIC_INCLUDE_ADC_H_ */
