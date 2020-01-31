/*
 * MSP430 hardware abstraction layer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef HAL_MSP430_INCLUDE_BITS_HAL_H_
#define HAL_MSP430_INCLUDE_BITS_HAL_H_

#include <in430.h>
#include <chip.h>
#include <stdint.h>

static inline __istate_t __enter_critical(void) {
    __istate_t istate = _get_interrupt_state();
    if (istate & GIE) {
        _disable_interrupts();
        _no_operation();
    }
    return istate;
}

static inline void __leave_critical(__istate_t istate) {
    if (istate & GIE) {
        _no_operation();
        _enable_interrupts();
    }
}

#endif /* HAL_MSP430_INCLUDE_BITS_HAL_H_ */
