/*
 * Log module
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP_MON_LOG_H
#define MSP_MON_LOG_H

int log_init(void);
int log_printf(const char* fmt, ...);
int log_update(void);

#define LOG_CONTINUE_UPDATE     (1)
#define LOG_WOULD_BLOCK         (2)

#define COL_GREY    "\033[1;30m"
#define COL_RED     "\033[1;31m"
#define COL_GREEN   "\033[1;32m"
#define COL_YELLOW  "\033[1;33m"
#define COL_BLUE    "\033[1;34m"
#define COL_CYAN    "\033[1;36m"
#define COL_VIOLET  "\033[1;35m"
#define COL_NORMAL  "\033[0m"

#define LOG_DBG_COLOR       COL_GREY
#define LOG_INFO_COLOR      COL_NORMAL
#define LOG_WARN_COLOR      COL_YELLOW
#define LOG_ERR_COLOR       COL_RED
#define LOG_SUCCESS_COLOR   COL_GREEN

#ifdef LOG_SKIP_LINE_FEED
    #define LF
#else
    #define LF "\n"
#endif

#define log_debug(fmt, ...)     log_printf(LOG_DBG_COLOR     fmt COL_NORMAL LF, ##__VA_ARGS__)
#define log_info(fmt, ...)      log_printf(LOG_INFO_COLOR    fmt COL_NORMAL LF, ##__VA_ARGS__)
#define log_warn(fmt, ...)      log_printf(LOG_WARN_COLOR    fmt COL_NORMAL LF, ##__VA_ARGS__)
#define log_error(fmt, ...)     log_printf(LOG_ERR_COLOR     fmt COL_NORMAL LF, ##__VA_ARGS__)
#define log_success(fmt, ...)   log_printf(LOG_SUCCESS_COLOR fmt COL_NORMAL LF, ##__VA_ARGS__)

#endif /* MSP_MON_LOG_H */
