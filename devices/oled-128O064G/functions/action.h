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

#ifndef _OLED_FUNCTION_ACTION_H_
#define _OLED_FUNCTION_ACTION_H_

void oledfun_do_reboot(char *dst, int exec);


void oledfun_do_dcuapp_restart(char *dst, int exec);

#endif
