/*
 * Phoenix-RTOS
 *
 * Functions to be used by OLED-menu
 *
 * Copyright 2019 Phoenix Systems
 * Author: Andrzej Glowinski
 *
 * %LICENSE%
 */

#ifndef _OLED_FUNCTION_METERS_H_
#define _OLED_FUNCTION_METERS_H_


void oledfun_get_prime_meters(char *dst, int exec);


void oledfun_get_rs485_meters(char *dst, int exec);

#endif
