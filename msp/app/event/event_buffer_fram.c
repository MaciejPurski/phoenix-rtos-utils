/*
 * FRAM event buffer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <stddef.h>
#include <limits.h>

#include <fm25l04b.h>

#include <log.h>

#include "event_buffer_fram.h"

static int eventBufferFRAM_read(eventBufferFRAM_t *b, unsigned idx, event_t *event)
{
    uint8_t tmp[EVENT_SIZE_SERIALIZED];
    uint16_t addr = b->fram_addr + EVENT_SIZE_SERIALIZED*idx;

#if 1
    fm25l04b_read(addr, tmp, EVENT_SIZE_SERIALIZED);
#else
    int res = fm25l04b_read(addr, tmp, EVENT_SIZE_SERIALIZED);
    if (res < 0)
        return -1;
#endif

    event_deserialize(event, tmp);

    return 0;
}

static int eventBufferFRAM_write(eventBufferFRAM_t *b, unsigned idx, const event_t *event)
{
    uint8_t tmp[EVENT_SIZE_SERIALIZED];
    uint16_t addr = b->fram_addr + EVENT_SIZE_SERIALIZED*idx;

    event_serialize(event, tmp);

#if 1
    fm25l04b_write(addr, tmp, EVENT_SIZE_SERIALIZED);
#else
    int res = fm25l04b_write(addr, tmp, EVENT_SIZE_SERIALIZED);
    if (res < 0)
        return -1;
#endif

#if 1
    /* Verify */
    uint8_t tmp2[EVENT_SIZE_SERIALIZED];
    event_t event2;
    fm25l04b_read(addr, tmp2, EVENT_SIZE_SERIALIZED);

    event_deserialize(&event2, tmp2);

    if (event2.timestamp != event->timestamp) {
        log_error("eventBufferFRAM_write: verification failed (timestamp)");
        return -1;
    }

    if (event2.type != event->type) {
        log_error("eventBufferFRAM_write: verification failed (type)");
        return -1;
    }
#endif

    return 0;
}

static inline int eventBufferFRAM_isEmpty(eventBufferFRAM_t *b)
{
    return b->last == b->first && !b->full;
}

static inline int eventBufferFRAM_firstId(eventBufferFRAM_t *b)
{
    return b->lastId + 1 - eventBufferFRAM_getNumOfEvents(b);
}

#if 0
int eventBufferFRAM_init(void *buffer)
{
    unsigned i, min_idx = 0, max_idx = 0, event_found = 0;
    uint32_t min = UINT32_MAX, max = 0;
    event_t event;

    eventBufferFRAM_t *b = (eventBufferFRAM_t*)buffer;
    if (b == NULL)
        return -1;

    b->full = 0;

    /* Find buffer head, tail and number of events */
    for (i = 0; i < b->size; i++) {
        if (eventBufferFRAM_read(b, i, &event) < 0)
            return -1;
        if (event.type == EVENT_NONE || event.timestamp == (uint32_t)0)
            continue;

        log_debug("e[%u, %lu]-%u", event.type, event.timestamp, i);
        event_found = 1;

        if (event.timestamp > max) {
            max = event.timestamp;
            max_idx = i;
        }
        if (event.timestamp < min) {
            min = event.timestamp;
            min_idx = i;
        }
    }

    if (event_found) {
        b->first = min_idx;
        b->last = (max_idx + 1)%(b->size);

        if (b->last == b->first)
            b->full = 1;

    } else { /* No events found */
        b->first = 0;
        b->last = 0;
    }

    log_debug("f=%u l=%u", b->first, b->last);

    b->lastId = eventBufferFRAM_getNumOfEvents(b);

    return 0;
}
#else
int eventBufferFRAM_init(void *buffer)
{
    unsigned i, prev_valid = 0, first_found = 0, last_found = 0;
    int min_idx = -1;
    uint32_t min = UINT32_MAX;
    event_t event;

    eventBufferFRAM_t *b = (eventBufferFRAM_t*)buffer;
    if (b == NULL)
        return -1;

    b->first = 0;
    b->last = 0;
    b->full = 0;

    /* Find buffer head, tail and number of events */
    for (i = 0; i < b->size; i++) {
        if (eventBufferFRAM_read(b, i, &event) < 0)
            return -1;
        if (event.type == EVENT_NONE || event.timestamp == (uint32_t)0) {
            if (prev_valid) {
                last_found = 1;
                b->last = i;
            }
            prev_valid = 0;
        } else {
            if (!prev_valid && i != 0) {
                first_found = 1;
                b->first = i;
            }
            if (event.timestamp < min) {
                min = event.timestamp;
                min_idx = i;
            }
            prev_valid = 1;
        }
    }

    if (!first_found && !last_found && min_idx >= 0) {
        b->first = min_idx;
        b->last = min_idx;
        b->full = 1;
    }

    b->lastId = eventBufferFRAM_getNumOfEvents(b);

    return 0;
}
#endif

int eventBufferFRAM_getNumOfEvents(void *buffer)
{
    eventBufferFRAM_t *b = (eventBufferFRAM_t*)buffer;
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

int eventBufferFRAM_push(void *buffer, const event_t *event)
{
    eventBufferFRAM_t *b = (eventBufferFRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (b->full) {
        /* Overwrite first (oldest element) */
        b->first = (b->first + 1)%(b->size);
    }

    if (eventBufferFRAM_write(b, b->last, event) < 0)
        return -1;
    b->last = (b->last + 1)%(b->size);

    if (b->last == b->first)
        b->full = 1;

    b->lastId++;

    return 0;
}

int eventBufferFRAM_peak(void *buffer, event_t *event, unsigned *id)
{
    eventBufferFRAM_t *b = (eventBufferFRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (eventBufferFRAM_isEmpty(b)) {
        log_error("remove: buffer is empty");
        return -1;
    }

    if (event != NULL) {
        if (eventBufferFRAM_read(b, b->first, event) < 0)
            return -1;
    }

    if (id != NULL)
        *id = eventBufferFRAM_firstId(b);

    return 0;
}

int eventBufferFRAM_remove(void *buffer, unsigned id)
{
    eventBufferFRAM_t *b = (eventBufferFRAM_t*)buffer;
    if (b == NULL)
        return -1;

    if (eventBufferFRAM_isEmpty(b)) {
        log_error("remove: buffer is empty");
        return -1;
    }

    unsigned firstId = eventBufferFRAM_firstId(b);

    if (id > firstId) { /* There are unprocessed events before this one */
        log_error("remove: event ID higher than expected");
        return -1;
    }

    if (id < firstId) /* Already overwritten */
        return 0;

    if (b->full)
        b->full = 0;

    /* Clear event in FRAM */
    event_t event = {.type = EVENT_NONE, .timestamp = 0};
    if (eventBufferFRAM_write(b, b->first, &event) < 0)
        return -1;

    b->first = (b->first + 1)%(b->size);

    return 0;
}

void eventBufferFRAM_initBufferDesc(void *buffer, eventBufferDesc_t *desc)
{
    if (desc == NULL)
        return;

    desc->buffer = buffer;
    desc->init = eventBufferFRAM_init;
    desc->getNumOfEvents = eventBufferFRAM_getNumOfEvents;
    desc->push = eventBufferFRAM_push;
    desc->peak = eventBufferFRAM_peak;
    desc->remove = eventBufferFRAM_remove;
}
