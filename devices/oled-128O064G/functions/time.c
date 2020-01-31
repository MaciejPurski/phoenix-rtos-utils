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

#include <sys/time.h>
#include <string.h>
#include <time.h>

#include <ps_dcsap_service.h>

#include <ps_dlms/obis_code.h>
#include <ps_dlms/obis_code_defs.h>
#include <ps_dlms/dlms_message_ids.h>
#include <ps_dlms/dlms_query.h>
#include <ps_dlms/dlms_types.h>

#include <ps_dlms/ps_meter_time.h>
#include <ps_dlms/ps_meter_time_utils.h>

#include "helper.h"

#include "time.h"


#define UPDATE_CACHE (5 * 1000 * 1000)

static struct {
	time_t time;
	time_t last_update;
} time_common;


static int time_request(void)
{
	uint8_t *bufptr = oledfun_common.buf;
	ps_obis_code_t obis;

	obis = g_dlms_id_time;
	obis.attr = 0x2;

	*bufptr++ = DLMS_REQUEST_ID__GET;
	*bufptr++ = DLMS_REQUEST_SUBTYPE_ID__GET_REQUEST_NORMAL;
	*bufptr++ = 0xc1;

	bufptr += ps_obis_code_to_bytes(&obis, bufptr);
	*bufptr++ = 0x0; // access selector: not present

	return ps_dcsap_service_send_cmd(&oledfun_common.service, 0, oledfun_common.buf, bufptr - oledfun_common.buf);
}


static int time_respnse(void)
{
	ps_dlms_response_t response;
	ps_meter_datetime_t dt;

	if (oledfun_helper_getResponseData(oledfun_common.buf, OLEDFUN_QUERY_BUFFER_SIZE, &response) < 0)
		return -1;

	if (ps_meter_time_cosem_parse(&dt, response.data, response.data_len) < 0)
		return -1;

	time_common.time = ps_meter_time_to_seconds(&dt) + dt.deviation * 60;

	return 0;
}


static int time_update(void)
{
	time_t curr;

	gettime(&curr, NULL);

	if (curr < time_common.last_update + UPDATE_CACHE)
		return 0;

	if (time_request() < 0 || time_respnse() < 0)
		return -1;

	time_common.last_update = curr;

	return 0;
}


void oledfun_get_time(char *dst, int exec)
{
	struct tm tm;

	if (exec)
		return;

	if (time_update() < 0) {
		strcpy(dst, "ERROR");
		return;
	}

	gmtime_r(&time_common.time, &tm);

	strftime(dst, oledfun_common.max_len, "%d-%m-%Y %H:%M", &tm);

	return;
}
