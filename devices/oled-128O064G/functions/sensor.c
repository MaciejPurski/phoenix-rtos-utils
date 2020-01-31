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

#include <stdio.h>
#include <string.h>

#include <libsystem_incotex.h>

#include "helper.h"

#include "sensor.h"


void oledfun_get_tamper1(char *dst, int exec)
{
	int res;
	if (exec)
		return;

	res = inc_getTamper(INC_TAMPER_COVER_MAIN);
	if (res < 0) {
		strcpy(dst, "Tamper 1: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Tamper 1: %s", res == 0 ? "Closed" : "Open");
	return;
}


void oledfun_get_tamper2(char *dst, int exec)
{
	int res;
	if (exec)
		return;

	res = inc_getTamper(INC_TAMPER_COVER_CABLE);
	if (res < 0) {
		strcpy(dst, "Tamper 2: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Tamper 2: %s", res == 0 ? "Closed" : "Open");
	return;
}



void oledfun_get_magnetometer(char *dst, int exec)
{
	int res;
	if (exec)
		return;

	res = inc_getTamper(INC_TAMPER_MAG);
	if (res < 0) {
		strcpy(dst, "Magnetometer: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Magnetometer: %s", res == 0 ? "OK" : "Tamp.");
	return;
}


void oledfun_get_accelerometer(char *dst, int exec)
{
	int res;
	if (exec)
		return;

	res = inc_getTamper(INC_TAMPER_ACCEL);
	if (res < 0) {
		strcpy(dst, "Accelerometer: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Accelerometer: %s", res == 0 ? "OK" : "Tamp.");
	return;
}
