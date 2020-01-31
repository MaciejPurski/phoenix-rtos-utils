/*
 * LIS2DH12 driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "lis2dh12.h"

#include <stdlib.h>

#include <spi.h>
#include <gpio.h>
#include <log.h>
#include <board.h>
#include <event.h>
#include <utils.h>

#define LIS2DH12_WHO_AM_I       (0x0f)
#define LIS2DH12_WHO_AM_I_VAL   (0b00110011)

#define LIS2DH12_STATUS_REG_AUX (0x07)

/* Temperature data overrun */
#define LIS2DH12_TOR            (1 << 6)

/* Temperature new data available */
#define LIS2DH12_TDA            (1 << 2)

#define LIS2DH12_OUT_TEMP_L     (0x0c)
#define LIS2DH12_OUT_TEMP_H     (0x0d)
#define LIS2DH12_CTRL_REG1      (0x20)

/* Data rate selection */
#define LIS2DH12_ODR_POWER_DOWN ((0b0000) << 4)
#define LIS2DH12_ODR_1_HZ       ((0b0001) << 4)
#define LIS2DH12_ODR_10_HZ      ((0b0010) << 4)
#define LIS2DH12_ODR_25_HZ      ((0b0011) << 4)
#define LIS2DH12_ODR_50_HZ      ((0b0100) << 4)
#define LIS2DH12_ODR_100_HZ     ((0b0101) << 4)
#define LIS2DH12_ODR_200_HZ     ((0b0110) << 4)
#define LIS2DH12_ODR_400_HZ     ((0b0111) << 4)

/* Low-power mode enable */
#define LIS2DH12_LPEN           (1 << 3)

/* Axis enable */
#define LIS2DH12_ZEN            (1 << 2)
#define LIS2DH12_YEN            (1 << 1)
#define LIS2DH12_XEN            (1 << 0)

#define LIS2DH12_CTRL_REG4      (0x23)
#define LIS2DH12_CTRL_REG5      (0x24)

/* FIFO enable */
#define LIS2DH12_FIFO_EN        (1 << 6)

#define LIS2DH12_STATUS_REG     (0x27)
#define LIS2DH12_OUT_X_L        (0x28)
#define LIS2DH12_OUT_X_H        (0x29)
#define LIS2DH12_OUT_Y_L        (0x2a)
#define LIS2DH12_OUT_Y_H        (0x2b)
#define LIS2DH12_OUT_Z_L        (0x2c)
#define LIS2DH12_OUT_Z_H        (0x2d)
#define LIS2DH12_FIFO_CTRL_REG  (0x2e)

/* FIFO mode selection */
#define LIS2DH12_FM_BYPASS      ((0b00) << 6)
#define LIS2DH12_FM_FIFO        ((0b01) << 6)
#define LIS2DH12_FM_STREAM      ((0b10) << 6)
#define LIS2DH12_FM_STREAM2FIFO ((0b11) << 6)

#define LIS2DH12_FIFO_SRC_REG   (0x2f)

/* Get the current number of unread samples stored in the FIFO buffer */
#define LIS2DH12_GET_FSS(fifo_src_reg) (fifo_src_reg & 0x1f)

#define LIS2DH12_RW_BIT             (1 << 7) /* 1: read, 0: write. */
#define LIS2DH12_MS_BIT             (1 << 6) /* 1: increment address, 0: do not increment */

static uint8_t _fullScale = LIS2DH12_FS_2_G;

static inline void lis2dh12_chipSelect(void)
{
    gpio_write(LIS2DH12_CS_PORT, LIS2DH12_CS_PIN, 0);
}

static inline void lis2dh12_chipDeselect(void)
{
    gpio_write(LIS2DH12_CS_PORT, LIS2DH12_CS_PIN, 1);
}

static uint8_t lis2dh12_readReg(uint8_t addr)
{
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];

    tx_buf[0] = addr | LIS2DH12_RW_BIT;
    tx_buf[1] = 0;

    lis2dh12_chipSelect();

    spi_exchange(LIS2DH12_SPI_IDX, tx_buf, rx_buf, 2);

    lis2dh12_chipDeselect();

    return rx_buf[1];
}

static void lis2dh12_read(uint8_t addr, uint8_t *data, size_t len)
{
    addr |= LIS2DH12_RW_BIT | LIS2DH12_MS_BIT;

    lis2dh12_chipSelect();

    spi_exchange(LIS2DH12_SPI_IDX, &addr, NULL, 1);
    spi_exchange(LIS2DH12_SPI_IDX, NULL, data, len);

    lis2dh12_chipDeselect();
}

static int lis2dh12_writeReg(uint8_t addr, uint8_t val)
{
    uint8_t tx_buf[2];

    tx_buf[0] = addr;
    tx_buf[1] = val;

    lis2dh12_chipSelect();

    spi_exchange(LIS2DH12_SPI_IDX, tx_buf, NULL, 2);

    lis2dh12_chipDeselect();

#if 1
    /* Verify */
    if (lis2dh12_readReg(addr) != val)
        return -1;
#endif

    return 0;
}

int lis2dh12_init(void)
{
    uint8_t val;
    spi_cfg_t spi_cfg;

    spi_cfg.msb_first = 1;
    spi_cfg.cpha = 0;
    spi_cfg.cpol = 1;
    spi_init(LIS2DH12_SPI_IDX, &spi_cfg);

    /* Read and verify WHO_AM_I register's value */
    val = lis2dh12_readReg(LIS2DH12_WHO_AM_I);
    if (val != LIS2DH12_WHO_AM_I_VAL) {
        log_error("Invalid WHO_AM_I value (got 0x%x, expected 0x%x)", val, LIS2DH12_WHO_AM_I_VAL);
        return -1;
    }

    val = LIS2DH12_ODR_10_HZ | LIS2DH12_LPEN | LIS2DH12_XEN | LIS2DH12_YEN | LIS2DH12_ZEN;
    if (lis2dh12_writeReg(LIS2DH12_CTRL_REG1, val) < 0) {
        log_error("lis3mdl: failed to configure LIS2DH12_CTRL_REG1");
        return -1;
    }

    val = LIS2DH12_FIFO_EN;
    if (lis2dh12_writeReg(LIS2DH12_CTRL_REG5, val) < 0) {
        log_error("lis3mdl: failed to configure LIS2DH12_CTRL_REG5");
        return -1;
    }

    val = LIS2DH12_FM_STREAM;
    if (lis2dh12_writeReg(LIS2DH12_FIFO_CTRL_REG, val) < 0) {
        log_error("lis3mdl: failed to configure LIS2DH12_FIFO_CTRL_REG");
        return -1;
    }

    if (lis2dh12_setFullScale(LIS2DH12_FS_2_G) < 0) {
        log_error("lis3mdl: failed to set full scale");
        return -1;
    }

    return 0;
}

int lis2dh12_setFullScale(uint8_t fs)
{
    uint8_t val;

    val = lis2dh12_readReg(LIS2DH12_CTRL_REG4);

    val &= ~LIS2DH12_FS_MASK;
    val |= fs;

    if (lis2dh12_writeReg(LIS2DH12_CTRL_REG4, fs) < 0)
        return -1;

    _fullScale = fs;

    return 0;
}

void lis2dh12_readRawData(int16_t *x, int16_t *y, int16_t *z)
{
    uint8_t buf[6];

    lis2dh12_read(LIS2DH12_OUT_X_L, buf, sizeof(buf));

    /* WARNING!
     * The code below assumes that we are in low power mode (to be more
     * precise, that the data is 8-bit and left-justified) */
    *x = _twosComplementToInt16(buf[0] | (((uint16_t)buf[1]) << 8)) / (1 << 8);
    *y = _twosComplementToInt16(buf[2] | (((uint16_t)buf[3]) << 8)) / (1 << 8);
    *z = _twosComplementToInt16(buf[4] | (((uint16_t)buf[5]) << 8)) / (1 << 8);
}

void lis2dh12_readData(int16_t *x, int16_t *y, int16_t *z)
{
    int16_t _x, _y, _z;
    uint8_t mgPerLSB = lis2dh12_getSensitivity();

    lis2dh12_readRawData(&_x, &_y, &_z);

    *x = _x * mgPerLSB;
    *y = _y * mgPerLSB;
    *z = _z * mgPerLSB;
}

uint8_t lis2dh12_getSensitivity(void)
{
    /* WARNING!
     * This function assumes that we are in low power mode */
    switch (_fullScale) {
    case LIS2DH12_FS_4_G:
        return 32;
    case LIS2DH12_FS_8_G:
        return 64;
    case LIS2DH12_FS_16_G:
        return 192;
    default:
        return 16;
    }
}

static vector3_t _fifo[LIS2DH12_FIFO_SIZE];
static uint16_t _fifo_first = 0;
static uint16_t _fifo_initialized = 0;

static int _event_sent = 0;

int lis2dh12_update(void)
{
    uint8_t val, samplesInFIFO;
    vector3_t curr;
    float diff; /* Magnitude of acceleration change */

    /* Check FIFO status */
    val = lis2dh12_readReg(LIS2DH12_FIFO_SRC_REG);
    samplesInFIFO = LIS2DH12_GET_FSS(val);

    /* Read data if available */
    if (samplesInFIFO > 0) {
        lis2dh12_readData(&curr.x, &curr.y, &curr.z);

        if (_fifo_initialized) {

            /* Check if orientation changed */
            diff = vector3_magnitude(vector3_sub(curr, _fifo[_fifo_first]));
            if (diff > LIS2DH12_CHANGE_THRESHOLD) {
                if (!_event_sent)
                    event_addNow(EVENT_ACCEL_ORIENTATION);
                _event_sent = 1;
            } else {
                _event_sent = 0;
            }
        }

        _fifo[_fifo_first] = curr;
        _fifo_first = (_fifo_first + 1) % LIS2DH12_FIFO_SIZE;

        if (!_fifo_initialized && _fifo_first == 0)
            _fifo_initialized = 1; /* FIFO is filled up */
    }

    return 0;
}

