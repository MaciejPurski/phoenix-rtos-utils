/*
 * FM25L04B driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "fm25l04b.h"

#include <spi.h>
#include <gpio.h>
#include <rtc.h>
#include <log.h>

#include <board.h>

#define FM25L04B_WREN               (0b0110)
#define FM25L04B_WRDI               (0b0100)
#define FM25L04B_RDSR               (0b0101)
#define FM25L04B_WRSR               (0b0001)
#define FM25L04B_READ(addr_msb)     (0b0011 | ((addr_msb & 1) << 3))
#define FM25L04B_WRITE(addr_msb)    (0b0010 | ((addr_msb & 1) << 3))

/* Status register bits */
#define WRITE_ENABLE_LATCH          (1 << 1)
#define BLOCK_PROTECTION_0          (1 << 2)
#define BLOCK_PROTECTION_1          (1 << 3)

#define ADDR_MSB_BIT(addr)          ((addr & (1 << 8)) >> 8)

#define MEMORY_SIZE                 (512) /* bytes */

static const uint8_t magicBytes[] = {0xDE, 0xAD};

#define MAGIC_BYTES_ADDR            (MEMORY_SIZE - sizeof(magicBytes))

static inline void fm25l04b_chipSelect(void)
{
    gpio_write(FM25L04B_CS_PORT, FM25L04B_CS_PIN, 0);
}

static inline void fm25l04b_chipDeselect(void)
{
    gpio_write(FM25L04B_CS_PORT, FM25L04B_CS_PIN, 1);
}

static void fm25l04b_readStatusReg(uint8_t *val) __attribute__((unused));

static void fm25l04b_readStatusReg(uint8_t *val)
{
    uint8_t tx_buf[2];
    uint8_t rx_buf[2];

    tx_buf[0] = FM25L04B_RDSR;
    tx_buf[1] = 0;

    fm25l04b_chipSelect();

    spi_exchange(FM25L04B_SPI_IDX, tx_buf, rx_buf, 2);

    fm25l04b_chipDeselect();

    *val = rx_buf[1];
}

void fm25l04b_writeEnable(void)
{
    uint8_t opcode = FM25L04B_WREN;

    fm25l04b_chipSelect();

    spi_exchange(FM25L04B_SPI_IDX, &opcode, NULL, 1);

    fm25l04b_chipDeselect();
}

void fm25l04b_read(uint16_t addr, uint8_t *data, size_t size)
{
    uint8_t header[2];

    header[0] = FM25L04B_READ(ADDR_MSB_BIT(addr));
    header[1] = addr & 0xff;

    fm25l04b_chipSelect();

    spi_exchange(FM25L04B_SPI_IDX, header, NULL, 2);
    spi_exchange(FM25L04B_SPI_IDX, NULL, data, size);

    fm25l04b_chipDeselect();
}

void fm25l04b_write(uint16_t addr, const uint8_t *data, size_t size)
{
    uint8_t header[2];

    fm25l04b_writeEnable();

    header[0] = FM25L04B_WRITE(ADDR_MSB_BIT(addr));
    header[1] = addr & 0xff;

    fm25l04b_chipSelect();

    spi_exchange(FM25L04B_SPI_IDX, header, NULL, 2);
    spi_exchange(FM25L04B_SPI_IDX, data, NULL, size);

    fm25l04b_chipDeselect();
}

int fm25l04b_verifyMagicBytes(void)
{
    uint8_t i, tmp[sizeof(magicBytes)];
    fm25l04b_read(MAGIC_BYTES_ADDR, tmp, sizeof(magicBytes));
    for (i = 0; i < sizeof(magicBytes); i++) {
        if (tmp[i] != magicBytes[i])
            return -1;
    }

    return 0;
}

int fm25l04b_format(void)
{
    /* Clear memory */
    fm25l04b_write(0x0, NULL, MEMORY_SIZE - sizeof(magicBytes));

    /* Write magic bytes */
    fm25l04b_write(MAGIC_BYTES_ADDR, magicBytes, sizeof(magicBytes));

    return fm25l04b_verifyMagicBytes();
}

int fm25l04b_init(void)
{
    int res;
    spi_cfg_t spi_cfg;

    fm25l04b_powerUp();

    spi_cfg.msb_first = 1;
    spi_cfg.cpha = 1;
    spi_cfg.cpol = 0;
    spi_init(FM25L04B_SPI_IDX, &spi_cfg);

    if (fm25l04b_verifyMagicBytes()) {
        log_debug("fm25l04b_init: magic bytes' verification failed. Formating...");
        res = fm25l04b_format();
        if (res < 0)
            log_error("fm25l04b_init: failed to format");
            return -1;
    }

    return 0;
}

void fm25l04b_powerUp(void)
{
    gpio_init(FM25L04B_PWRD_PORT, FM25L04B_PWRD_PIN, gpio_mode__out, gpio_pull__none);
    gpio_setDriveStrength(FM25L04B_PWRD_PORT, FM25L04B_PWRD_PIN, gpio_strength__high);
    gpio_write(FM25L04B_PWRD_PORT, FM25L04B_PWRD_PIN, 1);

    /* According to datasheet we need to wait at least 1 ms after powering up */
    rtc_sleepMs(2);
}

void fm25l04b_powerDown(void)
{
    gpio_init(FM25L04B_PWRD_PORT, FM25L04B_PWRD_PIN, gpio_mode__in, gpio_pull__none);
}
