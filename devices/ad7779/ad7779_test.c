/*
 * Phoenix-RTOS
 *
 * i.MX RT1064 besmart AD7779 test
 *
 * Copyright 2019 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <stdio.h>
#include <sys/msg.h>
#include <unistd.h>
#include "../../phoenix-rtos-devices/multi/imxrt-multi/imxrt-multi.h"
#include "ad7779.h"


int main(int argc, char *argv[])
{
	ad7779_init();
	ad7779_print_status();
	return 0;
}
