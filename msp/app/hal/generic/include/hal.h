/*
 * Hardware abstraction layer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP_MON_HAL_H
#define MSP_MON_HAL_H

#include <stdint.h>
#include <bits/hal.h>

int hal_init(void);
void hal_enterBootloader(void);
void hal_enterStandbyMode(void);

int hal_getResetReason(void);
const char *hal_getResetReasonAsString(void);

/* Returns 0 if 32kHz clock is stable */
int hal_clock32kHzFault(void);

#endif /* MSP_MON_HAL_H */
