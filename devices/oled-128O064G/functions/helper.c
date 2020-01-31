/*
 * Phoenix-RTOS
 *
 * Functions helper for OLED-menu
 *
 * Copyright 2019 Phoenix Systems
 * Author: Andrzej Glowinski
 *
 * %LICENSE%
 */


#include <ps_dlms/dlms_result.h>
#include <ps_dlms/dlms_query.h>

#include "helper.h"
#include "../oled-functions.h"

oledfun_common_t oledfun_common;


int oledfun_helper_getResponseData(uint8_t* buf, uint32_t len, ps_dlms_response_t *response) {
	ps_dlms_response_layout_t layout;

	int res, err;

	res = ps_dcsap_service_receive_response(&oledfun_common.service, &err, buf, len);
	res = ps_dlms_analyze_response_layout(buf, res, &layout);

	if (res < 0)
		return -1;

	if (layout.subcommands_cnt != 1)
		return -1;

	ps_dlms_get_response_from_get_data_result(layout.body, layout.body_len, response);

	if (response->dlms_result != DLMS_RESULT_SUCCESS)
		return -1;

	return 0;
}


int oledfun_helper_getResponseDataTable(uint8_t* buf, uint32_t len, uint32_t expected_cnt, ps_dlms_response_t *response) {
	ps_dlms_response_layout_t layout;
	ps_dlms_response_iter_t resp_iter;
	int res, err, i;

	res = ps_dcsap_service_receive_response(&oledfun_common.service, &err, buf, len);
	res = ps_dlms_analyze_response_layout(buf, res, &layout);

	if (res < 0)
		return -1;

	if (layout.subcommands_cnt != expected_cnt)
		return -1;

	ps_dlms_response_setup_iter(&resp_iter, &layout);

	for (i = 0; i < expected_cnt; ++i)
	{
		res = ps_dlms_response_iter_get_next_response(&resp_iter, &response[i]);
		if (res <= 0)
			return -1;

		if (response[i].dlms_result != DLMS_RESULT_SUCCESS)
			return -1;
	}

	return 0;
}
