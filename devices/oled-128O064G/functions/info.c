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
#include <stdio.h>

#include <libsystem_incotex.h>

#include <ps_dcsap_service.h>

#include <ps_dlms/obis_code.h>
#include <ps_dlms/obis_code_defs.h>
#include <ps_dlms/dlms_message_ids.h>
#include <ps_dlms/dlms_query.h>
#include <ps_dlms/dlms_types.h>

#include <ps_dcu_cosem/dcsap.h>

#include "helper.h"

#include "info.h"

#define UPDATE_SERIAL_CACHE (30ULL * 1000 * 1000)
#define UPDATE_VERSION_CACHE (3600ULL * 1000 * 1000)

static struct {
	char serial[17];
	time_t last_serial_update;
	char version[61];
	int version_len;
	char version_date[20];
	time_t last_version_update;
} info_common;


static int serial_request(void)
{
	uint8_t *bufptr = oledfun_common.buf;
	ps_obis_code_t obis = ps_dcu_dcsap_obis__dcu_dev_id;

	*bufptr++ = DLMS_REQUEST_ID__GET;
	*bufptr++ = DLMS_REQUEST_SUBTYPE_ID__GET_REQUEST_NORMAL;
	*bufptr++ = 0xc1;

	bufptr += ps_obis_code_to_bytes(&obis, bufptr);
	*bufptr++ = 0x0; // access selector: not present
	return ps_dcsap_service_send_cmd(&oledfun_common.service, 0, oledfun_common.buf, bufptr - oledfun_common.buf);
}


static int serial_response(void)
{
	ps_dlms_response_t response;
	const uint8_t *strptr;
	uint32_t strptr_len;

	if (oledfun_helper_getResponseData(oledfun_common.buf, OLEDFUN_QUERY_BUFFER_SIZE, &response) < 0)
		return -1;

	if (ps_dlms_octet_string_from_bytes(response.data, response.data_len, &strptr, &strptr_len) < 0)
		return -1;

	strncpy(info_common.serial, ((const char*)strptr), sizeof(info_common.serial) - 1);
	info_common.serial[sizeof(info_common.serial) - 1] = '\0';

	return 0;
}


static int serial_update(void)
{
	time_t curr;

	gettime(&curr, NULL);

	if (curr < info_common.last_serial_update + UPDATE_SERIAL_CACHE && info_common.last_serial_update != 0)
		return 0;

	if (serial_request() < 0 || serial_response() < 0)
		return -1;

	info_common.last_serial_update = curr;

	return 0;
}


static int version_request(void)
{
	uint8_t *bufptr = oledfun_common.buf;
	ps_obis_code_t obis = ps_dcu_dcsap_obis__dcu_fw_ver;

	*bufptr++ = DLMS_REQUEST_ID__GET;
	*bufptr++ = DLMS_REQUEST_SUBTYPE_ID__GET_REQUEST_NORMAL;
	*bufptr++ = 0xc1;

	bufptr += ps_obis_code_to_bytes(&obis, bufptr);
	*bufptr++ = 0x0; // access selector: not present
	return ps_dcsap_service_send_cmd(&oledfun_common.service, 0, oledfun_common.buf, bufptr - oledfun_common.buf);
}


static int version_response(void)
{
	ps_dlms_response_t response;
	const uint8_t *strptr;
	uint32_t strptr_len;
	int i;

	if (oledfun_helper_getResponseData(oledfun_common.buf, OLEDFUN_QUERY_BUFFER_SIZE, &response) < 0)
		return -1;

	if (ps_dlms_octet_string_from_bytes(response.data, response.data_len, &strptr, &strptr_len) < 0)
		return -1;

	for (i = strptr_len; i >= 0; --i) {
		if (strptr[i] == ',')
			break;
	}

	if (i > 0) {
		strncpy(info_common.version_date, (const char *)strptr + i + 2, MIN(sizeof(info_common.version_date), strptr_len - (i + 2)));
		strptr_len = i;
	}
	else {
		*info_common.version_date = '\0';
	}

	strncpy(info_common.version, (const char  *)strptr, MIN(sizeof(info_common.version), strptr_len));
	info_common.version[sizeof(info_common.version) - 1] = '\0';
	info_common.version_len = MIN(strptr_len, sizeof(info_common.version));

	return 0;
}


static int version_update(void)
{
	time_t curr;
	int res;

	gettime(&curr, NULL);

	if (curr < info_common.last_version_update + UPDATE_VERSION_CACHE && info_common.last_version_update != 0)
		return 0;

	if (version_request() < 0 || (res = version_response()) < 0)
		return -1;

	info_common.last_version_update = curr;

	return 0;
}


void oledfun_get_sn(char *dst, int exec)
{
	if (exec)
		return;

	if (serial_update() < 0) {
		strcpy(dst, "SN: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "SN: %s", info_common.serial);

	return;
}


void oledfun_get_battery(char *dst, int exec)
{
	int res;
	if (exec)
		return;

	res = inc_getBattery();
	if (res < 0) {
		strcpy(dst, "Battery: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Battery: %d.%dV", res / 1000, res % 1000);
	return;
}


void oledfun_get_msp_status(char *dst, int exec)
{
	if (exec)
		return;

	if (inc_getMSPtatus())
		strcpy(dst, "MSP: OK");
	else
		strcpy(dst, "MSP: ERROR");
	return;
}


void oledfun_get_plc_status(char *dst, int exec)
{
	if (exec)
		return;

	if (inc_getPLCStatus())
		strcpy(dst, "PLC: OK");
	else
		strcpy(dst, "PLC: ERROR");
	return;
}


void oledfun_get_version_date(char *dst, int exec)
{
	if (exec)
		return;

	if (version_update() < 0 || *info_common.version_date == '\0') {
		strcpy(dst, "ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "%s", info_common.version_date);

	return;
}


void oledfun_get_version_line1(char *dst, int exec)
{
	if (exec)
		return;

	if (version_update() < 0) {
		strcpy(dst, "ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "%s", info_common.version);

	return;
}


void oledfun_get_version_line2(char *dst, int exec)
{
	if (exec)
		return;

	if (version_update() < 0) {
		strcpy(dst, "ERROR");
		return;
	}

	if (info_common.version_len > oledfun_common.max_len - 1)
		snprintf(dst, oledfun_common.max_len, "%s", info_common.version + oledfun_common.max_len - 1);
	else
		*dst = '\0';

	return;
}


void oledfun_get_version_line3(char *dst, int exec)
{
	if (exec)
		return;

	if (version_update() < 0) {
		strcpy(dst, "ERROR");
		return;
	}

	if (info_common.version_len > 2 * (oledfun_common.max_len - 1))
		snprintf(dst, oledfun_common.max_len, "%s", info_common.version + 2 * (oledfun_common.max_len - 1));
	else
		*dst = '\0';

	return;
}
