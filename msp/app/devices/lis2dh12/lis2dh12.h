/*
 * LIS2DH12 driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef DEVICES_LIS2DH12_LIS2DH12_H_
#define DEVICES_LIS2DH12_LIS2DH12_H_

#include <stdint.h>

#define LIS2DH12_FIFO_SIZE              (20)
#define LIS2DH12_CHANGE_THRESHOLD       (500.0f) /* mg */

/* Full-scale selection */
#define LIS2DH12_FS_2_G         ((0b00) << 4)
#define LIS2DH12_FS_4_G         ((0b01) << 4)
#define LIS2DH12_FS_8_G         ((0b10) << 4)
#define LIS2DH12_FS_16_G        ((0b11) << 4)

#define LIS2DH12_FS_MASK        ((0b11) << 4)

int lis2dh12_init(void);
int lis2dh12_setFullScale(uint8_t fs);
void lis2dh12_readRawData(int16_t *x, int16_t *y, int16_t *z);

/* Read acceleration in mg */
void lis2dh12_readData(int16_t *x, int16_t *y, int16_t *z);

uint8_t lis2dh12_getSensitivity(void);
int lis2dh12_update(void);

#endif /* DEVICES_LIS2DH12_LIS2DH12_H_ */
