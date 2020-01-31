/*
 * RAM event buffer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <stddef.h>

#include "event_buffer_ram.h"

#ifdef EVENT_BUFFER_RAM_DEBUG
    #include <stdio.h>

    #define COL_RED     "\033[1;31m"
    #define COL_CYAN    "\033[1;36m"
    #define COL_YELLOW  "\033[1;33m"
    #define COL_NORMAL  "\033[0m"

    #define LOG_TAG "eventBufferRAM: "
    #define log_debug(fmt, ...)     do { printf(LOG_TAG fmt "\n", ##__VA_ARGS__); } while (0)
    #define log_info(fmt, ...)      do { printf(COL_CYAN LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
    #define log_warn(fmt, ...)      do { printf(COL_YELLOW LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
    #define log_error(fmt, ...)     do { printf(COL_RED  LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
#else
    #define log_debug(...)
    #define log_info(...)
    #define log_error(...)
    #define log_warn(...)
#endif

static inline int eventBufferRAM_isEmpty(eventBufferRAM_t *b)
{
    return b->last == b->first && !b->full;
}

static inline int eventBufferRAM_firstId(eventBufferRAM_t *b)
{
    return b->lastId + 1 - eventBufferRAM_getNumOfEvents(b);
}

int eventBufferRAM_init(void *buffer)
{
    eventBufferRAM_t *b = (eventBufferRAM_t*)buffer;
    if (b == NULL)
        return -1;

    b->first = 0;
    b->last = 0;
    b->full = 0;

    b->lastId = 0;

    return 0;
}

int eventBufferRAM_getNumOfEvents(void *buffer)
{
    eventBufferRAM_t *b = (eventBufferRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (b->full)
        return b->size;

    if (b->last >= b->first) {
        return b->last - b->first;
    } else {
        return (b->size - b->first) + b->last;
    }
}

int eventBufferRAM_push(void *buffer, const event_t *event)
{
    eventBufferRAM_t *b = (eventBufferRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (b->full) {
        /* Overwrite first (oldest element) */
        b->first = (b->first + 1)%(b->size);
    }

    b->mem[b->last] = *event;
    b->last = (b->last + 1)%(b->size);

    if (b->last == b->first)
        b->full = 1;

    b->lastId++;

    return 0;
}

int eventBufferRAM_peak(void *buffer, event_t *event, unsigned *id)
{
    eventBufferRAM_t *b = (eventBufferRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (eventBufferRAM_isEmpty(b)) {
        log_error("remove: buffer is empty");
        return -1;
    }

    if (event != NULL)
        *event = b->mem[b->first];

    if (id != NULL)
        *id = eventBufferRAM_firstId(b);

    return 0;
}

int eventBufferRAM_remove(void *buffer, unsigned id)
{
    eventBufferRAM_t *b = (eventBufferRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (eventBufferRAM_isEmpty(b)) {
        log_error("remove: buffer is empty");
        return -1;
    }

    unsigned firstId = eventBufferRAM_firstId(b);

    if (id > firstId) { /* There are unprocessed events before this one */
        log_error("remove: event ID higher than expected");
        return -1;
    }

    if (id < firstId) /* Already overwritten */
        return 0;

    if (b->full)
        b->full = 0;

    b->first = (b->first + 1)%(b->size);

    return 0;
}

void eventBufferRAM_initBufferDesc(void *buffer, eventBufferDesc_t *desc)
{
    if (desc == NULL)
        return;

    desc->buffer = buffer;
    desc->init = eventBufferRAM_init;
    desc->getNumOfEvents = eventBufferRAM_getNumOfEvents;
    desc->push = eventBufferRAM_push;
    desc->peak = eventBufferRAM_peak;
    desc->remove = eventBufferRAM_remove;
}
