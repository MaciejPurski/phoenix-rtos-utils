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

#ifndef _OLED_FUNCTIONS_H_
#define _OLED_FUNCTIONS_H_

#include <ps_dcsap_service.h>


#define OLEDFUN_QUERY_BUFFER_SIZE 128

typedef struct oledfun_common_s {
	unsigned int max_len;
	ps_dcsap_service_state_t service;
	uint8_t buf[OLEDFUN_QUERY_BUFFER_SIZE];
} oledfun_common_t;


typedef void (*oledfun_t)(char *, int);


oledfun_t oledfun_handleFunction(const char *name);


int oledfun_status(void);


int oledfun_update(void);


int oledfun_dcsapInit(void);


int oledfun_init(unsigned int len);


#endif
