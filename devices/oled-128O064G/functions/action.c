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

#include <string.h>
#include <sys/reboot.h>

#include "helper.h"

#include "action.h"


void oledfun_do_reboot(char *dst, int exec)
{
	if (exec) {
		reboot(PHOENIX_REBOOT_MAGIC);
	}

	strcpy(dst, "Reboot DCU");
	return;
}
