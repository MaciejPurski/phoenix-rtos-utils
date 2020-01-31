/*
 * MSP430 programming tool
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <unistd.h>

#include "bsl/include/bsl.h"
#include "bsl/include/bsl_defs.h"

#include "ihex/ihex.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <log.h>

/* Record handlers for Intel HEX parser */
static int _data_record_handler(uint16_t addr, const uint8_t *data, unsigned len);
static int _eof_record_handler(uint16_t addr, const uint8_t *data, unsigned len);
static int _ssa_record_handler(uint16_t addr, const uint8_t *data, unsigned len);

static int mark_app_as_valid(void);

static int load_pc = 0;
static uint32_t pc_to_load;

int main(int argc, char *argv[])
{
    int res, display_usage = 0;
    int invert_reset = 0, invert_test = 0, no_entry_seq = 0, use_syslog = 0, log_level = LOG_NOTICE;
    char *device = NULL;
    FILE *hex_file = NULL;

    while ((res = getopt(argc, argv, "rte:sd:Sl:")) >= 0) {
        switch (res) {
        case 'r':
            invert_reset = 1;
            break;
        case 't':
            invert_test = 1;
            break;
        case 'e':
            load_pc = 1;
            pc_to_load = (int)strtol(optarg, NULL, 0);
            break;
        case 's':
            no_entry_seq = 1;
            break;
        case 'd':
            device = optarg;
            break;
        case 'S':
            use_syslog = 1;
            break;
        case 'l':
            log_level = (int)strtol(optarg, NULL, 0);
            break;
        default:
            display_usage = 1;
            break;
        }

    }

    if (optind + 1 > argc) display_usage = 1;

    if (display_usage) {
        printf("Usage: msp-mon-prog [-rtsSl] [-d device] [-e program_counter] hex_file \n");
        printf("    -r                   Invert RESET signal\n");
        printf("    -t                   Invert TEST signal\n");
        printf("    -s                   Don't generate BSL entry sequence\n");
        printf("    -d device            Specifies device name\n");
        printf("    -e program_counter   Specifies program counter to load after flashing\n");
        printf("    -S                   Output logs to syslog instead of stdout\n\r");
        printf("    -l level             Log level (default: %u, debug: %u)\n\r", log_level, LOG_DEBUG);
        goto fail;
    }

    log_init(log_level, use_syslog);

    hex_file = fopen(argv[optind], "r");
    if (!hex_file) {
        log_error("Failed to open HEX file.");
        goto fail;
    }

    log_notice("Connecting to the BSL (%s)...", device);
    res = bsl_open(device, NULL, no_entry_seq, invert_test, invert_reset);
    if (res < 0) {
        log_error("An error occurred while trying to connect to the BSL.");
        goto fail;
    }

    ihex_cfg_t ihex_cfg;
    memset(&ihex_cfg, 0, sizeof(ihex_cfg));
    ihex_cfg.data_record = _data_record_handler;
    ihex_cfg.eof_record = _eof_record_handler;
    ihex_cfg.ssa_record = _ssa_record_handler;

    log_notice("Flashing...");
    res = ihex_parse(&ihex_cfg, hex_file);
    if (res == IHEX_RES_HANDLER_FAIL) {
        log_error("Programming failed.");
        goto fail2;
    } else if (res != IHEX_RES_OK) {
        log_error("An error occurred while parsing HEX file (%d).", res);
        goto fail2;
    }

    log_notice("Marking app as valid by writing magic words...");
    res = mark_app_as_valid();
    if (res < 0) {
        log_error("Marking app as valid failed.");
        goto fail2;
    }

    if (load_pc) {
        log_notice("Loading program counter (0x%06x)...", pc_to_load);
        res = bsl_load_pc(pc_to_load);
        if (res < 0) {
            log_error("Loading program counter failed.");
            goto fail2;
        }
    }

    log_success("MSP430 flashed successfully.");

    bsl_close();

    if (hex_file) fclose(hex_file);

    return 0;

fail2:
    bsl_close();

fail:
    if (hex_file) fclose(hex_file);

    return 1;
}

#define BUFFER_SIZE            (256)

static uint8_t buffer[BUFFER_SIZE];
static unsigned buffer_nextIdx = 0; /* Index of the next empty slot */
static uint16_t buffer_firstAddr = 0;

static int data_write(uint16_t addr, const uint8_t *data, unsigned len)
{
    int res;

    log_info("Erase check, program and verify (%u bytes at 0x%06x).", len, addr);

    if (BSL_MAGIC_BYTES_START <= (addr + len - 1) && addr <= BSL_MAGIC_BYTES_END) {
        log_error("Invalid binary (overlaps magic bytes).");
        return -1;
    }

    res = bsl_erase_check(addr, len);
    if (res < 0) {
        log_error("Erase check failed (%u bytes at 0x%06x).", len, addr);
        return res;
    }

    res = bsl_program(addr, data, len);
    if (res < 0) {
        log_error("An error occurred while writing (%u bytes at 0x%06x).", len, addr);
        return res;
    }

    res = bsl_verify(addr, data, len);
    if (res < 0) {
        log_error("Verification failed (%u bytes at 0x%06x).", len, addr);
        return res;
    }

    return 0;
}

static int buffer_flush(void)
{
    int res;
    unsigned tries = 4;

    if (buffer_nextIdx == 0)
        return 0;

    while (1) {
        res = data_write(buffer_firstAddr, buffer, buffer_nextIdx);
        if (res == 0)
            break;

        log_error("Writing data failed (%u tries left).", tries);

        if (tries-- == 0)
            return res;
    }

    buffer_nextIdx = 0;

    return 0;
}

static int buffer_push(uint16_t addr, const uint8_t *data, unsigned len)
{
    unsigned i;

    if (buffer_nextIdx == 0) {
        buffer_firstAddr = addr;
    } else if (addr != (buffer_firstAddr + buffer_nextIdx) || (buffer_nextIdx + len) > BUFFER_SIZE) {
        return -1;
    }

    for (i = 0; i < len; i++)
        buffer[buffer_nextIdx++] = data[i];

    return 0;
}

static int _data_record_handler(uint16_t addr, const uint8_t *data, unsigned len)
{
    int res;

    res = buffer_push(addr, data, len);
    if (res == 0)
        return 0;

    /* Failed to push data to buffer. Flush and try again */

    res = buffer_flush();
    if (res != 0) {
        log_error("buffer_flush failed (%d)", res);
        return -1;
    }

    return buffer_push(addr, data, len);
}

static int _eof_record_handler(uint16_t addr, const uint8_t *data, unsigned len)
{
    (void)addr;
    (void)data;
    (void)len;

    buffer_flush();

    return 0;
}

static int _ssa_record_handler(uint16_t addr, const uint8_t *data, unsigned len)
{
    (void)addr;

    if (len != 4)
        return -1;

    if (!load_pc) {
        load_pc = 1;
        pc_to_load = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    } else {
        log_warn("Initial PC already defined. Omitting.");
    }

    return 0;
}

static int mark_app_as_valid(void)
{
    int res;
    uint16_t addr = BSL_MAGIC_BYTES_START;
    uint8_t magic_bytes[BSL_NUM_OF_MAGIC_BYTES] = {0xde, 0xad, 0xbe, 0xef};

    res = bsl_program(addr, magic_bytes, sizeof(magic_bytes));
    if (res < 0) {
        log_error("An error occurred while writing (%lu bytes at 0x%06x).", sizeof(magic_bytes), addr);
        return res;
    }

    res = bsl_verify(addr, magic_bytes, sizeof(magic_bytes));
    if (res < 0) {
        log_error("Verification failed (%lu bytes at 0x%06x).", sizeof(magic_bytes), addr);
        return res;
    }

    return 0;
}
