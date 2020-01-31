/*
 * MSP430 RTC driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef HAL_GENERIC_INCLUDE_RTC_H_
#define HAL_GENERIC_INCLUDE_RTC_H_

#include <stdint.h>

extern volatile uint16_t rtc_systime;

int rtc_init(void);

int rtc_getUnixTime(uint32_t *time);
int rtc_setUnixTime(uint32_t time);

uint16_t rtc_getSysTime(void);

void rtc_sleepMs(uint16_t ms);

#endif /* HAL_GENERIC_INCLUDE_RTC_H_ */
