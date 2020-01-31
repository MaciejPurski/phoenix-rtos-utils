/*
 * FM25L04B driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef DEVICES_FM25L04B_FM25L04B_H_
#define DEVICES_FM25L04B_FM25L04B_H_

#include <stdint.h>
#include <stdlib.h>

int fm25l04b_init(void);
void fm25l04b_read(uint16_t addr, uint8_t *data, size_t size);
void fm25l04b_write(uint16_t addr, const uint8_t *data, size_t size);
void fm25l04b_powerUp(void);
void fm25l04b_powerDown(void);

#endif /* DEVICES_FM25L04B_FM25L04B_H_ */
