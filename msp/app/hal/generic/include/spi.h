/*
 * SPI driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP_MON_HAL_SPI_H
#define MSP_MON_HAL_SPI_H

#include <hal.h>

#define SPI_RES__OK                             (0)
#define SPI_RES__INVALID_IDX                    (-1)
#define SPI_RES__ARG_ERROR                      (-2)

typedef struct {
    /* Clock phase
     * 0: data changed on first clock edge and captured on the following
     * 1: data captured on first clock edge and changed on the following */
    uint8_t cpha;

    /* Clock polarity
     * 0: inactive low
     * 1: inactive high */
    uint8_t cpol;

    /* 0: LSB first
     * 1: MSB first */
    uint8_t msb_first;
} spi_cfg_t;

int spi_init(int idx, const spi_cfg_t *cfg);
int spi_exchange(int idx, const uint8_t *in, uint8_t *out, uint16_t len);
int spi_deinit(int idx);

#endif /* MSP_MON_HAL_SPI_H */
