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

#ifndef _OLED_FUNCTION_HELPER_H_
#define _OLED_FUNCTION_HELPER_H_

#include <ps_dlms/dlms_query.h>
#include "../oled-functions.h"

#define FLAG(X) (1 << X)
#define MIN(X, Y) (X < Y ? X : Y)


extern oledfun_common_t oledfun_common;


int oledfun_helper_getResponseData(uint8_t* buf, uint32_t len, ps_dlms_response_t *response);


int oledfun_helper_getResponseDataTable(uint8_t* buf, uint32_t len, uint32_t expected_cnt, ps_dlms_response_t *response);


#endif
