/*
 * Log module
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "log.h"

#include <stdio.h>
#include <stdarg.h>

static int log_lvl = LOG_ERR;
static int log_use_syslog = 0;

void log_init(int lvl, int use_syslog)
{
    log_lvl = lvl;
    log_use_syslog = use_syslog;

    if (use_syslog)
        openlog("msp-mon-prog", LOG_NDELAY, LOG_DAEMON);
}

void log_printf(int lvl, const char* fmt, ...)
{
    va_list arg;

    if (lvl > log_lvl)
        return;

    va_start(arg, fmt);
    if (!log_use_syslog) {
        vprintf(fmt, arg);
    } else {
        vsyslog(lvl, fmt, arg);
    }
    va_end(arg);
}

void log_print_buffer(int lvl, const uint8_t *buf, unsigned len)
{
    unsigned i;

    if (lvl > log_lvl)
        return;

    for (i = 0; i < len; i++) {
        if (i == 0) log_printf(lvl, "\033[1;30m[ \033[0m");

        log_printf(lvl, "\033[1;30m%02x \033[0m", buf[i]);

        if ((i % 16) == 15 && i != (len - 1))
            log_printf(lvl, "\033[1;30m\n  \033[0m");

        if (i == (len - 1))
            log_printf(lvl, "\033[1;30m]\n\033[0m");
    }
}
