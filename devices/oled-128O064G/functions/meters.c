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
#include <stdio.h>
#include <string.h>

#include <ps_dcsap_service.h>

#include <ps_dlms/obis_code.h>
#include <ps_dlms/obis_code_defs.h>
#include <ps_dlms/dlms_message_ids.h>
#include <ps_dlms/dlms_query.h>
#include <ps_dlms/dlms_types.h>
#include <ps_dlms/medium_types.h>

#include <ps_dcu_cosem/dcsap.h>

#include "helper.h"

#include "meters.h"


#define UPDATE_CACHE (5 * 1000 * 1000)


static struct {
	uint16_t data[ps_dlms_medium_type__cnt][3];
	time_t last_update;
} meters_common;


static int meters_request(void)
{
	uint8_t *bufptr = oledfun_common.buf;
	ps_obis_code_t obis = ps_dcu_dcsap_obis__meter_cumulative_stats;
	obis.attr = 0x3;

	*bufptr++ = DLMS_REQUEST_ID__GET;
	*bufptr++ = DLMS_REQUEST_SUBTYPE_ID__GET_REQUEST_NORMAL;
	*bufptr++ = 0xc1;

	bufptr += ps_obis_code_to_bytes(&obis, bufptr);
	*bufptr++ = 0x0; // access selector: not present

	return ps_dcsap_service_send_cmd(&oledfun_common.service, 0, oledfun_common.buf, bufptr - oledfun_common.buf);
}


static int meters_response(void)
{
	int i, j, res;
	ps_dlms_response_t response;
	uint32_t struct_elem_cnt = 0, data_elem_cnt = 0;

	res = oledfun_helper_getResponseData(oledfun_common.buf, OLEDFUN_QUERY_BUFFER_SIZE, &response);
	if (res < 0)
		return -1;

	res = ps_dlms_struct_from_bytes(response.data, response.data_len, &struct_elem_cnt);
	if (res < 0 || struct_elem_cnt != 4)
		return -1;

	response.data += res;
	response.data_len -= res;
	
	for (i = 0; i < struct_elem_cnt; ++i) {
		res = ps_dlms_struct_from_bytes(response.data, response.data_len, &data_elem_cnt);
		if (res < 0 || data_elem_cnt != 3)
			return -1;
		
		response.data += res;
		response.data_len -= res;
		
		for (j = 0; j < data_elem_cnt; ++j) {
			res = ps_dlms_uint16_from_bytes(response.data, response.data_len, &meters_common.data[i][j]);
			if (res < 0)
				return -1;

			response.data += res;
			response.data_len -= res;
		}
	}

	return 0;
}


static int meters_update(void)
{
	time_t curr; 

	gettime(&curr, NULL);

	if (curr < meters_common.last_update + UPDATE_CACHE)
		return 0;

	if (meters_request() < 0 || meters_response() < 0)
		return -1;

	meters_common.last_update = curr;

	return 0;

}


void oledfun_get_prime_meters(char *dst, int exec)
{
	int connected, available = 0, i;

	if (exec)
		return;

	if (meters_update() < 0) {
		strcpy(dst, "PRIME: ERROR");
		return;
	}

	for (i = 0; i < 3; ++i)
		available += meters_common.data[ps_dlms_medium_type__plc][i];

	connected = meters_common.data[ps_dlms_medium_type__plc][2];

	snprintf(dst, oledfun_common.max_len, "PRIME: %d/%d", connected, available);

	return;
}


void oledfun_get_rs485_meters(char *dst, int exec)
{
	int connected, available = 0, i;

	if (exec)
		return;

	if (meters_update() < 0) {
		strcpy(dst, "RS485: ERROR");
		return;
	}

	for (i = 0; i < 3; ++i)
		available += meters_common.data[ps_dlms_medium_type__rs485][i];

	connected = meters_common.data[ps_dlms_medium_type__rs485][2];

	snprintf(dst, oledfun_common.max_len, "RS485: %d/%d", connected, available);

	return;
}


