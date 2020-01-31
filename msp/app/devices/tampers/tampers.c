/*
 * Tampers
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "tampers.h"

#include <gpio.h>
#include <board.h>
#include <event.h>
#include <log.h>
#include <state.h>

struct tamper_def_s {
    int port;
    int pin;
    int open_state;
    uint8_t event_open;
    uint8_t event_closed;
    int state_flag;
};

static struct tamper_def_s tampers[] = {
     {
          TAMPER1_PORT,
          TAMPER1_PIN,
          TAMPER1_OPEN_STATE,
          EVENT_TAMPER_1_START,
          EVENT_TAMPER_1_STOP,
          mmp_state_flag__tampered_1,
     },
     {
          TAMPER2_PORT,
          TAMPER2_PIN,
          TAMPER2_OPEN_STATE,
          EVENT_TAMPER_2_START,
          EVENT_TAMPER_2_STOP,
          mmp_state_flag__tampered_2,
     },
};

void tampers_init(void)
{
    /* Configure tampers */
    gpio_init(TAMPER1_PORT, TAMPER1_PIN, gpio_mode__in, gpio_pull__none);
    gpio_init(TAMPER2_PORT, TAMPER2_PIN, gpio_mode__in, gpio_pull__none);
}

int tampers_update(void)
{
    int i, res = 0;
    unsigned curr_state, prev_state;
    uint8_t event;

    for (i = 0; i < sizeof(tampers)/sizeof(struct tamper_def_s); i++) {

        /* Get current state */
        curr_state = (gpio_read(tampers[i].port, tampers[i].pin) == tampers[i].open_state);

        /* Get previous state */
        state_getFlag(tampers[i].state_flag, &prev_state);

        /* Add appropriate event if state changed */
        if (curr_state != prev_state) {

            if (curr_state) {
                event = tampers[i].event_open;
            } else {
                event = tampers[i].event_closed;
            }

            if (event_addNow(event) < 0) {
                log_error("failed to add event (state=%d, tamper=%d, event=%u)", curr_state, i, event);
                res = -1;
            }
        }

        /* Store current state */
        state_setFlag(tampers[i].state_flag, curr_state);
    }

    return res;
}
