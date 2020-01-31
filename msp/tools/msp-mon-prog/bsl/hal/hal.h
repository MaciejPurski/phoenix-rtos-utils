/*
 * MSP430 BSL programming library HAL
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP430_BSL_PROG_BSL_HAL_H
#define MSP430_BSL_PROG_BSL_HAL_H

#include <stdint.h>

int bsl_hal_init(void);

int bsl_hal_serial_open(const char *device);
int bsl_hal_serial_write(const uint8_t *data, uint32_t len);
int bsl_hal_serial_read(uint8_t *data, uint32_t len, uint32_t timeout_ms);
int bsl_hal_serial_close(void);

int bsl_hal_set_tst_state(int state, int inverted);
int bsl_hal_set_rst_state(int state, int inverted);

int bsl_hal_sleep_ms(unsigned ms);

#endif // #ifndef MSP430_BSL_PROG_BSL_HAL_H
