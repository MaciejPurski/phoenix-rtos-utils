/*
 * Log module - msp_mon_prot based implementation
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <log.h>
#include <msp_mon_prot.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_BUFFER_LEN                  (512)
#define LOG_MAX_MSG_LEN                 (128)

typedef struct {
    char data[LOG_BUFFER_LEN];
    unsigned first; /* First character in buffer */
    unsigned end; /* First empty space */
    unsigned full;
} log_buffer_t;

log_buffer_t log_buffer;

static int log_buffer_init(log_buffer_t *l);
static int log_buffer_push(log_buffer_t *l, char *buffer);
static int log_buffer_pop(log_buffer_t *l, char *buffer, unsigned len);
static int log_buffer_is_empty(log_buffer_t *l);

int log_init(void)
{
    return log_buffer_init(&log_buffer);
}

int log_printf(const char* fmt, ...)
{
    char msg[LOG_MAX_MSG_LEN];
    va_list arg;

    va_start(arg, fmt);
    int res = vsnprintf(msg, LOG_MAX_MSG_LEN, fmt, arg);
    va_end(arg);

    if (res < 0)
        return -1;

    return log_buffer_push(&log_buffer, msg);
 }

int log_update(void)
{
    int res;

    if (log_buffer_is_empty(&log_buffer))
        return 0;

    mmp_t *mmp = mmp_get_default_instance();
    if (!mmp)
        return -1;

    if (!mmp_is_ready_to_transmit(mmp))
        return LOG_WOULD_BLOCK;

    char msg[LOG_MAX_MSG_LEN];
    res = log_buffer_pop(&log_buffer, msg, sizeof(msg));
    if (res < 0)
        return res;

    res = mmp_transmit(mmp, MMP_CMD__LOG_MSG, (uint8_t*)msg, res, NULL, NULL, 0);
    if (res < 0)
        return -1;

    return log_buffer_is_empty(&log_buffer) ? 0 : LOG_CONTINUE_UPDATE;
}

static int log_buffer_init(log_buffer_t *l)
{
    if (!l)
        return -1;

    l->first = 0;
    l->end = 0;
    l->full = 0;

    return 0;
}

static int log_buffer_get_available_space(log_buffer_t *l)
{
    if (l->full) {
        return 0;
    } else if (l->first <= l->end) {
        return sizeof(l->data) - l->end + l->first;
    } else {
        return l->first - l->end;
    }
}

static int log_buffer_push(log_buffer_t *l, char *buffer)
{
    unsigned needed = strnlen(buffer, LOG_MAX_MSG_LEN) + 1;
    if (needed == 1) /* Drop if that's an empty string */
        return 0;
    if (needed > LOG_MAX_MSG_LEN) /* Drop if no terminating character was found */
        return -1;

    unsigned available = log_buffer_get_available_space(l);
    if (needed > available) /* Drop if there's not enough space */
        return -1;

    if (needed == available)
        l->full = 1;

    unsigned end = l->end;
    while (needed--) {
        l->data[end] = *buffer++;
        end = (end + 1)%sizeof(l->data);
    }

    l->end = end;

    return 0;
}

static int log_buffer_pop(log_buffer_t *l, char *buffer, unsigned size)
{
    char *org_buffer = buffer;

    if (log_buffer_is_empty(&log_buffer))
        return -1;

    unsigned first = l->first;
    while (size--) {
        char c = l->data[first];
        first = (first + 1)%sizeof(l->data);
        *buffer++ = c;

        if (c == '\0') {
            l->first = first;
            if (l->full) l->full = 0;
            return buffer - org_buffer;
        }

        if (first == l->end)
            return -2; /* Reached the end of the buffer, but no null character found */
    }

    /* Buffer is too small to contain the whole message */
    return -1;
}

static int log_buffer_is_empty(log_buffer_t *l)
{
    if (!l->full && l->first == l->end)
        return 1; /* Empty */

    return 0; /* Not empty */
}
