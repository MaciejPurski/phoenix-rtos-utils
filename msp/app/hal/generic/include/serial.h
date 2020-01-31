/*
 * Serial driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP_MON_HAL_SERIAL_H
#define MSP_MON_HAL_SERIAL_H

#include <stdint.h>

typedef enum {
    serial_baudrate__9600,
    serial_baudrate__19200,
    serial_baudrate__38400,
    serial_baudrate__57600,
    serial_baudrate__115200,
} serial_baudrate_t;

int serial_init(serial_baudrate_t baudrate);
int serial_is_tx_busy(void);
int serial_write(const uint8_t *data, uint16_t len);
int serial_read(uint8_t *data, uint16_t len);
int serial_deinit(void);

#endif /* MSP_MON_HAL_SERIAL_H */
