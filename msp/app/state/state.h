/*
 * Device state control
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef STATE_STATE_H_
#define STATE_STATE_H_

#include <stdint.h>
#include <msp_mon_prot.h>

#define STATE_RES__OK               (0)
#define STATE_RES__INVALID_STATE    (-1)
#define STATE_RES__INVLAID_FLAG     (-2)
#define STATE_RES__INTERNAL_ERR     (-3)

int state_setFlag(unsigned flag, unsigned status);
int state_getFlag(unsigned flag, unsigned *status);

mmp_state_flags_t state_get(void);

/* Store current device state in non-volatile memory */
int state_store(void);

/* Try to restore previous device state from non-volatile memory */
int state_tryToRestore(void);

#endif /* STATE_STATE_H_ */
