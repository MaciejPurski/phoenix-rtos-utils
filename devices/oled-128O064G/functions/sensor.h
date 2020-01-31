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

#ifndef _OLED_FUNCTION_SENSOR_H_
#define _OLED_FUNCTION_SENSOR_H_

void oledfun_get_tamper1(char *dst, int exec);


void oledfun_get_tamper2(char *dst, int exec);


void oledfun_get_magnetometer(char *dst, int exec);


void oledfun_get_accelerometer(char *dst, int exec);


#endif
