/*
 * MSP430 SPI driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <spi.h>
#include <hal.h>

#include <stddef.h>

typedef struct {
    uint8_t ctl1;
    uint8_t ctl0;
    uint8_t reserved0[4];
    uint8_t br0;
    uint8_t br1;
    uint8_t mctl;
    uint8_t reserved1;
    uint8_t stat;
    uint8_t reserved2;
    uint8_t rxbuf;
    uint8_t reserved3;
    uint8_t txbuf;
    uint8_t reserved4[13];
    uint8_t ie;
    uint8_t ifg;
    uint8_t iv;
} __attribute__((packed, aligned(1))) spi_regs_t;

typedef struct {
    volatile spi_regs_t * const regs;
    uint8_t * const pxsel; /* Port function select register */
    const uint8_t miso_pin;
    const uint8_t mosi_pin;
    const uint8_t clk_pin;
} spi_t;

#define NUM_OF_SPI_INSTANCES                    (2)

static spi_t spi[NUM_OF_SPI_INSTANCES] = {
    {(spi_regs_t*)&UCA1CTLW0, (uint8_t*)&P4SEL, 5, 4, 0},
    {(spi_regs_t*)&UCB0CTLW0, (uint8_t*)&P3SEL, 1, 0, 2},
};

int spi_init(int idx, const spi_cfg_t *cfg)
{
    if (!cfg)
        return SPI_RES__ARG_ERROR;

    if (idx >= NUM_OF_SPI_INSTANCES)
        return SPI_RES__INVALID_IDX;

    uint8_t ctl0 = UCSYNC | UCMST;
    if (cfg->cpha) ctl0 |= UCCKPH;
    if (cfg->cpol) ctl0 |= UCCKPL;
    if (cfg->msb_first) ctl0 |= UCMSB;

    spi[idx].regs->ctl1 = UCSWRST;
    spi[idx].regs->ctl0 = ctl0;
    spi[idx].regs->ctl1 = UCSSEL__SMCLK;
    spi[idx].regs->br0 = 64;
    *spi[idx].pxsel |= (1 << spi[idx].miso_pin)
                    |  (1 << spi[idx].mosi_pin)
                    |  (1 << spi[idx].clk_pin);
    spi[idx].regs->ctl1 &= ~UCSWRST;

    return SPI_RES__OK;
}

int spi_exchange(int idx, const uint8_t *in, uint8_t *out, uint16_t len)
{
    uint8_t *org_out = out;

    if (idx >= NUM_OF_SPI_INSTANCES)
        return SPI_RES__INVALID_IDX;

    while (len--) {
        while (!(spi[idx].regs->ifg & UCTXIFG));
        if (in != NULL) {
            spi[idx].regs->txbuf = *in++;
        } else {
            spi[idx].regs->txbuf = 0;
        }
        while (!(spi[idx].regs->ifg & UCRXIFG));
        if (out != NULL) {
            *out++ = spi[idx].regs->rxbuf;
        } else {
            (void)spi[idx].regs->rxbuf;
        }
    }

    return out - org_out;
}

int spi_deinit(int idx)
{
    if (idx >= NUM_OF_SPI_INSTANCES)
        return SPI_RES__INVALID_IDX;

    spi[idx].regs->ctl1 = UCSWRST;
    spi[idx].regs->ctl0 &= ~UCMST;


    *spi[idx].pxsel &= ~((1 << spi[idx].miso_pin)
                         |(1 << spi[idx].mosi_pin)
                         |(1 << spi[idx].clk_pin));

    return SPI_RES__OK;
}
