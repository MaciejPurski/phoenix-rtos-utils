/*
 * LIS3MDL driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "lis3mdl.h"

#include <stdlib.h>

#include <spi.h>
#include <gpio.h>
#include <log.h>
#include <board.h>
#include <event.h>
#include <utils.h>
#include <state.h>

#define LIS3MDL_WHO_AM_I        (0x0f)
#define LIS3MDL_WHO_AM_I_VAL    (0b00111101)

#define LIS3MDL_CTRL_REG1       (0x20)

/* Temperature sensor enable */
#define LIS3MDL_TEMP_EN         (1 << 7)

/* X and Y axes operative mode selection */
#define LIS3MDL_OM_LP           ((0b00) << 5)
#define LIS3MDL_OM_MP           ((0b01) << 5)
#define LIS3MDL_OM_HP           ((0b10) << 5)
#define LIS3MDL_OM_UHP          ((0b11) << 5)

/* Output Data Rate */
#define LIS3MDL_DO_0_625_HZ     ((0b000) << 2)
#define LIS3MDL_DO_1_25_HZ      ((0b001) << 2)
#define LIS3MDL_DO_2_5_HZ       ((0b010) << 2)
#define LIS3MDL_DO_5_HZ         ((0b011) << 2)
#define LIS3MDL_DO_10_HZ        ((0b100) << 2)
#define LIS3MDL_DO_20_HZ        ((0b101) << 2)
#define LIS3MDL_DO_40_HZ        ((0b110) << 2)
#define LIS3MDL_DO_80_HZ        ((0b111) << 2)

#define LIS3MDL_CTRL_REG2       (0x21)
#define LIS3MDL_CTRL_REG3       (0x22)

/* Low-power mode configuration */
#define LIS3MDL_LP              (1 << 5)

#define LIS3MDL_CTRL_REG4       (0x23)
#define LIS3MDL_CTRL_REG5       (0x24)
#define LIS3MDL_STATUS_REG      (0x27)
#define LIS3MDL_OUT_X_L         (0x28)
#define LIS3MDL_OUT_X_H         (0x29)
#define LIS3MDL_OUT_Y_L         (0x2a)
#define LIS3MDL_OUT_Y_H         (0x2b)
#define LIS3MDL_OUT_Z_L         (0x2c)
#define LIS3MDL_OUT_Z_H         (0x2d)
#define LIS3MDL_TEMP_OUT_L      (0x2e)
#define LIS3MDL_TEMP_OUT_H      (0x2f)

#define LIS3MDL_INT_CFG         (0x30)

/* Enable interrupt generation */
#define LIS3MDL_XIEN           (1 << 7)
#define LIS3MDL_YIEN           (1 << 6)
#define LIS3MDL_ZIEN           (1 << 5)
#define LIS3MDL_IEN            (1 << 0)

#define LIS3MDL_INT_SRC         (0x31)
#define LIS3MDL_INT_THS_L       (0x32)
#define LIS3MDL_INT_THS_H       (0x33)

#define LIS3MDL_RW_BIT              (1 << 7) /* 1: read, 0: write. */
#define LIS3MDL_MS_BIT              (1 << 6) /* 1: increment address, 0: do not increment */

static uint8_t _fullScale;

static inline void lis3mdl_chipSelect(void)
{
    gpio_write(LIS3MDL_CS_PORT, LIS3MDL_CS_PIN, 0);
}

static inline void lis3mdl_chipDeselect(void)
{
    gpio_write(LIS3MDL_CS_PORT, LIS3MDL_CS_PIN, 1);
}

static uint8_t lis3mdl_readReg(uint8_t addr)
{
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];

    tx_buf[0] = addr | LIS3MDL_RW_BIT;
    tx_buf[1] = 0;

    lis3mdl_chipSelect();

    spi_exchange(LIS3MDL_SPI_IDX, tx_buf, rx_buf, 2);

    lis3mdl_chipDeselect();

    return rx_buf[1];
}

static int lis3mdl_writeReg(uint8_t addr, uint8_t val)
{
    uint8_t tx_buf[2];

    tx_buf[0] = addr;
    tx_buf[1] = val;

    lis3mdl_chipSelect();

    spi_exchange(LIS3MDL_SPI_IDX, tx_buf, NULL, 2);

    lis3mdl_chipDeselect();

#if 1
    /* Verify */
    if (lis3mdl_readReg(addr) != val)
        return -1;
#endif

    return 0;
}

int lis3mdl_init(uint8_t mode)
{
    uint8_t val;
    spi_cfg_t spi_cfg;

    spi_cfg.msb_first = 1;
    spi_cfg.cpha = 0;
    spi_cfg.cpol = 1;
    spi_init(LIS3MDL_SPI_IDX, &spi_cfg);

    /* Read and verify WHO_AM_I register's value */
    val = lis3mdl_readReg(LIS3MDL_WHO_AM_I);
    if (val != LIS3MDL_WHO_AM_I_VAL) {
        log_error("lis3mdl: invalid WHO_AM_I value (got 0x%x, expected 0x%x)", val, LIS3MDL_WHO_AM_I_VAL);
        return -1;
    }

    if (lis3mdl_setFullScale(LIS3MDL_FS_16_GAUSS) < 0) {
        log_error("lis3mdl: failed to set full scale");
        return -1;
    }

    val = LIS3MDL_TEMP_EN | LIS3MDL_OM_HP | LIS3MDL_DO_10_HZ;
    if (lis3mdl_writeReg(LIS3MDL_CTRL_REG1, val) < 0) {
        log_error("lis3mdl: failed to configure LIS3MDL_CTRL_REG1");
        return -1;
    }

    if (lis3mdl_setMode(mode) < 0) {
        log_error("lis3mdl: failed to set mode");
        return -1;
    }

    if (lis3mdl_enableInterrupts(1, 1, 1, 0x1000) < 0) {
        log_error("lis3mdl: failed to enable interrupts");
        return -1;
    }

    return 0;
}

int lis3mdl_setFullScale(uint8_t fs)
{
    uint8_t val;

    val = lis3mdl_readReg(LIS3MDL_CTRL_REG2);

    val &= ~LIS3MDL_FS_MASK;
    val |= fs;

    if (lis3mdl_writeReg(LIS3MDL_CTRL_REG2, fs) < 0)
        return -1;

    _fullScale = fs;

    return 0;
}

int lis3mdl_setMode(uint8_t mode)
{
    uint8_t val;

    val = lis3mdl_readReg(LIS3MDL_CTRL_REG3);

    val &= ~LIS3MDL_MD_MASK;
    val |= mode;

    if (lis3mdl_writeReg(LIS3MDL_CTRL_REG3, val) < 0) {
        log_error("lis3mdl: failed to configure LIS3MDL_CTRL_REG3");
        return -1;
    }

    return 0;
}

static void lis3mdl_readAxisRaw(uint8_t reg_l, uint8_t reg_h, int16_t *val)
{
    uint8_t l, h;
    uint16_t tmp;

    l = lis3mdl_readReg(reg_l);
    h = lis3mdl_readReg(reg_h);
    tmp = (((uint16_t)h) << 8) | l;

    *val = _twosComplementToInt16(tmp);
}

void lis3mdl_readRaw(int16_t *x, int16_t *y, int16_t *z)
{
    lis3mdl_readAxisRaw(LIS3MDL_OUT_X_L, LIS3MDL_OUT_X_H, x);
    lis3mdl_readAxisRaw(LIS3MDL_OUT_Y_L, LIS3MDL_OUT_Y_H, y);
    lis3mdl_readAxisRaw(LIS3MDL_OUT_Z_L, LIS3MDL_OUT_Z_H, z);
}

void lis3mdl_readData(int16_t *x, int16_t *y, int16_t *z)
{
    int16_t _x, _y, _z;
    uint16_t LSBPerGauss = lis3mdl_getSensitivity();

    lis3mdl_readRaw(&_x, &_y, &_z);

    /* Multiply by 1000 to get the result in miligauss */
    *x = (((uint32_t)1000) *_x)/LSBPerGauss;
    *y = (((uint32_t)1000) *_y)/LSBPerGauss;
    *z = (((uint32_t)1000) *_z)/LSBPerGauss;
}

uint16_t lis3mdl_getSensitivity(void)
{
    /* WARNING!
     * This function assumes that we are in low power mode */
    switch (_fullScale) {
    case LIS3MDL_FS_4_GAUSS:
        return 6842;
    case LIS3MDL_FS_8_GAUSS:
        return 3421;
    case LIS3MDL_FS_12_GAUSS:
        return 2281;
    default:
        return 1711;
    }
}

void lis3mdl_readTempRaw(int16_t *temp)
{
    lis3mdl_readAxisRaw(LIS3MDL_TEMP_OUT_L, LIS3MDL_TEMP_OUT_H, temp);
}

void lis3mdl_readTemp(int32_t *temp)
{
    int16_t _temp;
    const int32_t sensitivity = 125; /* degrees Celsius * 10^(-3) per LSB */
    const int32_t offset = 25000;

    lis3mdl_readTempRaw(&_temp);

    *temp = _temp * sensitivity + offset;
}

int lis3mdl_enableInterrupts(int x, int y, int z, uint16_t threshold)
{
    uint8_t val = LIS3MDL_IEN;

    if (lis3mdl_writeReg(LIS3MDL_INT_THS_L, threshold & 0xff) < 0)
        return -1;
    if (lis3mdl_writeReg(LIS3MDL_INT_THS_H, (threshold >> 8) & 0xff) < 0)
        return -1;

    if (x) val |= LIS3MDL_XIEN;
    if (y) val |= LIS3MDL_YIEN;
    if (z) val |= LIS3MDL_ZIEN;

    if (lis3mdl_writeReg(LIS3MDL_INT_CFG, val) < 0)
        return -1;

    return 0;
}

uint8_t lis3mdl_getInterruptStatus(void)
{
    return lis3mdl_readReg(LIS3MDL_INT_SRC);
}

uint8_t lis3mdl_getStatusReg(void)
{
    return lis3mdl_readReg(LIS3MDL_STATUS_REG);
}

int lis3mdl_update(void)
{
    unsigned curr_state, prev_state;
    uint8_t status = lis3mdl_getInterruptStatus();

    /* X axis */
    curr_state = LIS3MDL_ALARM_X(status);
    state_getFlag(mmp_state_flag__mag_alarm_x, &prev_state);
    if (curr_state && !prev_state) event_addNow(EVENT_MAG_X_START);
    if (!curr_state && prev_state) event_addNow(EVENT_MAG_X_STOP);
    state_setFlag(mmp_state_flag__mag_alarm_x, curr_state);

    /* Y axis */
    curr_state = LIS3MDL_ALARM_Y(status);
    state_getFlag(mmp_state_flag__mag_alarm_y, &prev_state);
    if (curr_state && !prev_state) event_addNow(EVENT_MAG_Y_START);
    if (!curr_state && prev_state) event_addNow(EVENT_MAG_Y_STOP);
    state_setFlag(mmp_state_flag__mag_alarm_y, curr_state);

    /* Z axis */
    curr_state = LIS3MDL_ALARM_Z(status);
    state_getFlag(mmp_state_flag__mag_alarm_z, &prev_state);
    if (curr_state && !prev_state) event_addNow(EVENT_MAG_Z_START);
    if (!curr_state && prev_state) event_addNow(EVENT_MAG_Z_STOP);
    state_setFlag(mmp_state_flag__mag_alarm_z, curr_state);

    return 0;
}
