/*
 * Intel HEX format parser
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "ihex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <log.h>

#define IHEX_RECORD_TYPE_DATA               (0x00)
#define IHEX_RECORD_TYPE_END_OF_FILE        (0x01)
#define IHEX_RECORD_TYPE_ESA                (0x02) /* Extended Segment Address */
#define IHEX_RECORD_TYPE_SSA                (0x03) /* Start Segment Address */
#define IHEX_RECORD_TYPE_ELA                (0x04) /* Extended Linear Address */
#define IHEX_RECORD_TYPE_SLA                (0x05) /* Standard Linear Address */

static int _call_handler(const ihex_cfg_t *cfg,
                         uint8_t record_type,
                         uint16_t addr,
                         const uint8_t *data,
                         unsigned len);

static int _parse_hex_number(char *input, unsigned len, uint32_t *out);
static int _read_hex_number(FILE *file, unsigned len, uint32_t *out);
static int _read_byte(FILE *file, uint8_t *out, uint8_t *checksum);
static int _read_half_word(FILE *file, uint16_t *out, uint8_t *checksum);

int ihex_parse(const ihex_cfg_t *cfg, FILE *file)
{
    int res;
    char buffer[4];

    if (!cfg)
        return IHEX_RES_NULL_PARAM;

    while (1) {

        uint8_t checksum = 0;

        /* Read start code */
        while (1) {
            res = fread(buffer, 1, 1, file);
            if (res == 0 && feof(file)) {
                return IHEX_RES_OK;
            } else if (res < 1) {
                log_error("ihex_parse: fread failed (res=%d, ferror=%d)", res, ferror(file));
                return IHEX_RES_READ_ERROR;
            } else if (buffer[0] == '\r' || buffer[0] == '\n') {
                continue;
            } else if (buffer[0] == ':') {
                break;
            } else {
                log_error("ihex_parse: syntax error [%c]", buffer[0]);
                return IHEX_RES_SYNTAX_ERROR;
            }
        }

        /* Read byte count */
        uint8_t byte_count;
        if ((res = _read_byte(file, &byte_count, &checksum)) < 0)
            return res;

        /* Read address */
        uint16_t addr = 0;
        if ((res = _read_half_word(file, &addr, &checksum)) < 0)
            return res;

        /* Read record type */
        uint8_t record_type;
        if ((res = _read_byte(file, &record_type, &checksum)) < 0)
            return res;

        /* Read data */
        uint8_t data[byte_count], i;
        for (i = 0; i < byte_count; i++) {
            if ((res = _read_byte(file, &data[i], &checksum)) < 0)
                return res;
        }

        /* Read checksum */
        uint8_t read_checksum;
        if ((res = _read_byte(file, &read_checksum, NULL)) < 0)
            return res;

        /* Calculate two's complement of checksum and compare */
        checksum = (~checksum) + 1;
        if (checksum != read_checksum) {
            log_error("ihex_parse: checksum error");
            return IHEX_RES_CHECKSUM_ERROR;
        }

#if 0
        log_debug("byte_count = 0x%02x", byte_count);
        log_debug("addr = 0x%04x", addr);
        log_debug("record_type = 0x%02x", record_type);
        log_debug("checksum = 0x%02x", checksum);
        log_debug("data = ");
        print_buffer(data, byte_count);
#endif

        /* Choose appropriate handler for this record type */
        res = _call_handler(cfg, record_type, addr, data, byte_count);
        if (res < 0) {
            log_error("ihex_parse: handler failed (%d)", res);
            return res;
        }
    }
}

static int _call_handler(const ihex_cfg_t *cfg,
                         uint8_t record_type,
                         uint16_t addr,
                         const uint8_t *data,
                         unsigned len)
{
    int (*handler)(uint16_t addr, const uint8_t *data, unsigned len) = NULL;

    switch (record_type) {
    case IHEX_RECORD_TYPE_DATA:
        handler = cfg->data_record;
        break;
    case IHEX_RECORD_TYPE_END_OF_FILE:
        handler = cfg->eof_record;
        break;
    case IHEX_RECORD_TYPE_ESA:
        handler = cfg->esa_record;
        break;
    case IHEX_RECORD_TYPE_SSA:
        handler = cfg->ssa_record;
        break;
    case IHEX_RECORD_TYPE_ELA:
        handler = cfg->ela_record;
        break;
    case IHEX_RECORD_TYPE_SLA:
        handler = cfg->sla_record;
        break;
    default:
        return IHEX_RES_UNKNOWN_RECORD_TYPE;
    }

    if (handler) {
        int res = handler(addr, data, len);
        if (res != 0)
            return IHEX_RES_HANDLER_FAIL;
    }
#ifdef IHEX_ALL_HANDLERS_REQUIRED
    else {
        log_error("Missing handler (record_type = 0x%02x", record_type);
        return IHEX_RES_NULL_HANDLER;
    }
#endif

    return 0;
}

static int _parse_hex_number(char *input, unsigned len, uint32_t *out)
{
    uint32_t res = 0;

    while (len--) {
        char c = *input++;

        if (c >= '0' && c <= '9') {
            res = res*16 + c - '0';
        } else if (c >= 'a' && c <= 'f') {
            res = res*16 + c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            res = res*16 + c - 'A' + 10;
        } else {
            return -1;
        }
    }

    if (out) *out = res;

    return IHEX_RES_OK;
}

static int _read_hex_number(FILE *file, unsigned len, uint32_t *out)
{
    char buffer[8];

    size_t res = fread(buffer, 1, len, file);
    if (res < len && feof(file)) {
        log_error("_read_hex_number: unexpected end of file");
        return IHEX_RES_READ_ERROR;
    } else if (res < len) {
        log_error("_read_hex_number: fread failed (res=%d, ferror=%d)", res, ferror(file));
        return IHEX_RES_READ_ERROR;
    } else if (_parse_hex_number(buffer, len, out) != 0) {
        log_error("_read_hex_number: syntax error");
        return IHEX_RES_SYNTAX_ERROR;
    }

    return IHEX_RES_OK;
}

static int _read_byte(FILE *file, uint8_t *out, uint8_t *checksum)
{
    uint32_t tmp;

    int res = _read_hex_number(file, 2, &tmp);
    if (res < 0)
        return res;

    if (out) *out = tmp;
    if (checksum) {
        *checksum += tmp & 0xff;
    }

    return IHEX_RES_OK;
}

static int _read_half_word(FILE *file, uint16_t *out, uint8_t *checksum)
{
    uint32_t tmp;

    int res = _read_hex_number(file, 4, &tmp);
    if (res < 0)
        return res;

    if (out) *out = tmp;
    if (checksum) *checksum += (tmp & 0xff) + ((tmp >> 8) & 0xff);

    return IHEX_RES_OK;
}
