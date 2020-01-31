/*
 * Intel HEX format parser
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP430_BSL_PROG_IHEX_H
#define MSP430_BSL_PROG_IHEX_H

#include <stdint.h>
#include <stdio.h>

#define IHEX_ALL_HANDLERS_REQUIRED

#define IHEX_RES_OK                         (0)
#define IHEX_RES_SYNTAX_ERROR               (-1)
#define IHEX_RES_READ_ERROR                 (-2)
#define IHEX_RES_NULL_PARAM                 (-3)
#define IHEX_RES_HANDLER_FAIL               (-4)
#define IHEX_RES_UNKNOWN_RECORD_TYPE        (-5)
#define IHEX_RES_NULL_HANDLER               (-6)
#define IHEX_RES_CHECKSUM_ERROR             (-7)

typedef struct {

    /* Data record handler */
    int (*data_record)(uint16_t addr, const uint8_t *data, unsigned len);

    /* End of file record handler */
    int (*eof_record)(uint16_t addr, const uint8_t *data, unsigned len);

    /* Extended Segment Address record handler */
    int (*esa_record)(uint16_t addr, const uint8_t *data, unsigned len);

    /* Start Segment Address record handler */
    int (*ssa_record)(uint16_t addr, const uint8_t *data, unsigned len);

    /* Extended Linear Address record handler */
    int (*ela_record)(uint16_t addr, const uint8_t *data, unsigned len);

    /* Standard Linear Address record handler */
    int (*sla_record)(uint16_t addr, const uint8_t *data, unsigned len);

} ihex_cfg_t;

/* Parses Intel HEX file. Appropriate handler is called for each record (line). */
int ihex_parse(const ihex_cfg_t *cfg, FILE *file);

#endif // #ifndef MSP430_BSL_PROG_IHEX_H
