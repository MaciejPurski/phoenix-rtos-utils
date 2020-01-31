/*
 * Phoenix-RTOS
 *
 * Functions to be used by OLED-menu
 *
 * Copyright 2018 Phoenix Systems
 * Author: Andrzej Glowinski
 *
 * %LICENSE%
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <ps_time.h>
#include <ps_dcsap_service.h>

#include "oled-functions.h"

#include "functions/helper.h"

#include "functions/action.h"
#include "functions/eth.h"
#include "functions/info.h"
#include "functions/meters.h"
#include "functions/plc.h"
#include "functions/sensor.h"
#include "functions/time.h"


#define __FID(id, ptr) id


#define __FPTR(id, ptr) ptr


#define FID(OBJ) __FID(OBJ)


#define FPTR(OBJ) __FPTR(OBJ)


#define OLEDFUNC(X) #X, oledfun_ ## X


#define CHECK_FUNCTION(name, X) do { if(strcmp(name, FID(OLEDFUNC(X))) == 0) return FPTR(OLEDFUNC(X)); } while(0)


oledfun_t oledfun_handleFunction(const char *name)
{
	CHECK_FUNCTION(name, get_time);
	CHECK_FUNCTION(name, get_sn);
	CHECK_FUNCTION(name, get_prime_meters);
	CHECK_FUNCTION(name, get_rs485_meters);
	CHECK_FUNCTION(name, get_battery);
	CHECK_FUNCTION(name, get_msp_status);
	CHECK_FUNCTION(name, get_plc_status);
	CHECK_FUNCTION(name, get_version_date);
	CHECK_FUNCTION(name, get_version_line1);
	CHECK_FUNCTION(name, get_version_line2);
	CHECK_FUNCTION(name, get_version_line3);
	CHECK_FUNCTION(name, get_gsm_ifname);
	CHECK_FUNCTION(name, get_gsm_status);
	CHECK_FUNCTION(name, get_gsm_media);
	CHECK_FUNCTION(name, get_gsm_ip);
	CHECK_FUNCTION(name, get_gsm_apn);
	CHECK_FUNCTION(name, get_gsm_link);
	CHECK_FUNCTION(name, get_en1_ip);
	CHECK_FUNCTION(name, get_en1_mask);
	CHECK_FUNCTION(name, get_en1_status);
	CHECK_FUNCTION(name, get_en1_link);
	CHECK_FUNCTION(name, get_en1_type);
	CHECK_FUNCTION(name, get_en2_ip);
	CHECK_FUNCTION(name, get_en2_mask);
	CHECK_FUNCTION(name, get_en2_status);
	CHECK_FUNCTION(name, get_en2_link);
	CHECK_FUNCTION(name, get_en2_type);
	CHECK_FUNCTION(name, get_vpn_status);
	CHECK_FUNCTION(name, get_vpn_ifname);
	CHECK_FUNCTION(name, get_vpn_ip);
	CHECK_FUNCTION(name, get_vpn_link);
	CHECK_FUNCTION(name, get_plc_snr);
	CHECK_FUNCTION(name, get_plc_txbs);
	CHECK_FUNCTION(name, get_plc_txds);
	CHECK_FUNCTION(name, get_plc_txbf);
	CHECK_FUNCTION(name, get_plc_txdf);
	CHECK_FUNCTION(name, get_plc_rx);
	CHECK_FUNCTION(name, get_tamper1);
	CHECK_FUNCTION(name, get_tamper2);
	CHECK_FUNCTION(name, get_magnetometer);
	CHECK_FUNCTION(name, get_accelerometer);
	CHECK_FUNCTION(name, do_reboot);

	return NULL;
}


int oledfun_status(void)
{
	return oledfun_common.service.error_code;
}


int oledfun_update(void)
{
	ps_time_update();
	return ps_dcsap_service_update(&oledfun_common.service);
}


int oledfun_dcsapInit(void)
{
	ps_dcsap_service_init(&oledfun_common.service);

	if (ps_dcsap_service_connect(&oledfun_common.service) < 0)
		return -1;

	return 0;
}


int oledfun_init(unsigned int len)
{
	oledfun_common.max_len = len;

	ps_time_init();
	while (oledfun_dcsapInit() != 0)
		usleep(100000);

	return 0;
}
