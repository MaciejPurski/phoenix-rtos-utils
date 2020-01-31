/*
 * Log module
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP430_BSL_PROG_LOG_H
#define MSP430_BSL_PROG_LOG_H

#include <syslog.h>
#include <stdint.h>

void log_init(int lvl, int use_syslog);
void log_printf(int lvl, const char* fmt, ...);
void log_print_buffer(int lvl, const uint8_t *buf, unsigned len);

#define log_error(fmt, ...)     log_printf(LOG_ERR, "\033[1;31m" fmt "\033[0m\n", ##__VA_ARGS__)
#define log_warn(fmt, ...)      log_printf(LOG_WARNING, "\033[1;33m" fmt "\033[0m\n", ##__VA_ARGS__)
#define log_notice(fmt, ...)    log_printf(LOG_NOTICE, "\033[0m" fmt "\033[0m\n", ##__VA_ARGS__)
#define log_success(fmt, ...)   log_printf(LOG_NOTICE, "\033[1;32m" fmt "\033[0m\n", ##__VA_ARGS__)
#define log_info(fmt, ...)      log_printf(LOG_INFO, "\033[0m" fmt "\033[0m\n", ##__VA_ARGS__)
#define log_debug(fmt, ...)     log_printf(LOG_DEBUG, "\033[1;30m" fmt "\033[0m\n", ##__VA_ARGS__)

#define print_buffer(buf, len)  log_print_buffer(LOG_DEBUG, buf, len)

#endif // #ifndef MSP430_BSL_PROG_IHEX_H
