/*
 * LIS3MDL driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef DEVICES_LIS3MDL_LIS3MDL_H_
#define DEVICES_LIS3MDL_LIS3MDL_H_

#include <stdint.h>

/* Full Scale */
#define LIS3MDL_FS_4_GAUSS      ((0b00) << 5)
#define LIS3MDL_FS_8_GAUSS      ((0b01) << 5)
#define LIS3MDL_FS_12_GAUSS     ((0b10) << 5)
#define LIS3MDL_FS_16_GAUSS     ((0b11) << 5)

#define LIS3MDL_FS_MASK         ((0b11) << 5)

/* Operating mode selection */
#define LIS3MDL_MD_CONTINUOUS   (0b00)
#define LIS3MDL_MD_SINGLE       (0b01)
#define LIS3MDL_MD_POWER_DOWN   (0b10)

#define LIS3MDL_MD_MASK         (0b11)

/* Value on X-axis exceeds the threshold on the positive side */
#define LIS3MDL_PTH_X           (1 << 7)

/* Value on Y-axis exceeds the threshold on the positive side */
#define LIS3MDL_PTH_Y           (1 << 6)

/* Value on Z-axis exceeds the threshold on the positive side */
#define LIS3MDL_PTH_Z           (1 << 5)

/* Value on X-axis exceeds the threshold on the negative side */
#define LIS3MDL_NTH_X           (1 << 4)

/* Value on Y-axis exceeds the threshold on the negative side */
#define LIS3MDL_NTH_Y           (1 << 3)

/* Value on Z-axis exceeds the threshold on the negative side */
#define LIS3MDL_NTH_Z           (1 << 2)

/* Internal measurement range overflow on magnetic value */
#define LIS3MDL_MROI            (1 << 1)

/* This bit signals when an interrupt event occurs */
#define LIS3MDL_INT             (1 << 0)

#define LIS3MDL_ALARM_X(status)     (!!(status & (LIS3MDL_PTH_X | LIS3MDL_NTH_X)))
#define LIS3MDL_ALARM_Y(status)     (!!(status & (LIS3MDL_PTH_Y | LIS3MDL_NTH_Y)))
#define LIS3MDL_ALARM_Z(status)     (!!(status & (LIS3MDL_PTH_Z | LIS3MDL_NTH_Z)))

int lis3mdl_init(uint8_t mode);
int lis3mdl_setFullScale(uint8_t fs);
int lis3mdl_setMode(uint8_t mode);
void lis3mdl_readRaw(int16_t *x, int16_t *y, int16_t *z);

/* Read magnetic flux density in miligauss  */
void lis3mdl_readData(int16_t *x, int16_t *y, int16_t *z);

uint16_t lis3mdl_getSensitivity(void);
void lis3mdl_readTempRaw(int16_t *temp);
void lis3mdl_readTemp(int32_t *temp);
int lis3mdl_enableInterrupts(int x, int y, int z, uint16_t threshold);
uint8_t lis3mdl_getInterruptStatus(void);
uint8_t lis3mdl_getStatusReg(void);
int lis3mdl_update(void);
int lis3mdl_deinit(void);

#define lis3mdl_powerDown()     lis3mdl_setMode(LIS3MDL_MD_POWER_DOWN)
#define lis3mdl_powerUp()       lis3mdl_setMode(LIS3MDL_MD_CONTINUOUS)

#endif /* DEVICES_LIS3MDL_LIS3MDL_H_ */
