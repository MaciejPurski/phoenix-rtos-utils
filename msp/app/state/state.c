/*
 * Device state control
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "state.h"

#include <rtc.h>
#include <fm25l04b.h>

#include <stddef.h>
#include <string.h>

#define MAGIC_BYTE      (0xa8)

#define FRAM_MEM_ADDR   (0x0)
#define FRAM_MEM_SIZE   (0xa)

struct state_s {
    uint32_t unix_time;
    mmp_state_flags_t flags;
    uint8_t magic_byte; /* Appropriate value of the magic byte indicates that restored state is a valid one */
};

static struct state_s current_state = {.unix_time = 0, .flags = 0, .magic_byte = MAGIC_BYTE};

int state_setFlag(unsigned flag, unsigned status)
{
    if (flag >= sizeof(mmp_state_flags_t) * 8)
        return STATE_RES__INVLAID_FLAG;

    mmp_state_flags_t mask = ((mmp_state_flags_t)1) << flag;

    if (status) {
        current_state.flags |= mask;
    } else {
        current_state.flags &= ~mask;
    }

    return STATE_RES__OK;
}

int state_getFlag(unsigned flag, unsigned *status)
{
    if (flag >= sizeof(mmp_state_flags_t) * 8)
        return STATE_RES__INVLAID_FLAG;

    mmp_state_flags_t mask = ((mmp_state_flags_t)1) << flag;

    if (status != NULL)
        *status = !!(current_state.flags & mask);

    return STATE_RES__OK;
}

mmp_state_flags_t state_get(void)
{
    return current_state.flags;
}

int state_store(void)
{
    if (sizeof(struct state_s) > FRAM_MEM_SIZE)
        return STATE_RES__INTERNAL_ERR;

    rtc_getUnixTime(&current_state.unix_time);

    fm25l04b_write(FRAM_MEM_ADDR, (uint8_t*)&current_state, sizeof(struct state_s));

    /* TODO: Verify (?) */

    return STATE_RES__OK;
}

int state_tryToRestore(void)
{
    struct state_s restored;

    if (sizeof(struct state_s) > FRAM_MEM_SIZE)
        return STATE_RES__INTERNAL_ERR;

    fm25l04b_read(FRAM_MEM_ADDR, (uint8_t*)&restored, sizeof(struct state_s));

    if (restored.magic_byte != MAGIC_BYTE)
        return STATE_RES__INVALID_STATE;

    memcpy(&current_state, &restored, sizeof(struct state_s));

    rtc_setUnixTime(current_state.unix_time);

    return STATE_RES__OK;
}
