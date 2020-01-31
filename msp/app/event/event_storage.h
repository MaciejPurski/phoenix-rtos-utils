/*
 * Event storage
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef EVENT_STORAGE_H_
#define EVENT_STORAGE_H_

#include "event_defs.h"

typedef struct {
    void *buffer;
    int (*init)(void *buffer);
    int (*getNumOfEvents)(void *buffer);
    int (*push)(void *buffer, const event_t *event);
    int (*peak)(void *buffer, event_t *event, unsigned *id);
    int (*remove)(void *buffer, unsigned id);
} eventBufferDesc_t;

typedef struct {
    eventBufferDesc_t *buffers;
    unsigned num_of_buffers;

    unsigned dispatch_policy[NUM_OF_EVENT_TYPES];

    /* Buffer and ID of an event that is currently being sent */
    unsigned current_buffer;
    unsigned current_id;

    unsigned sending;
    unsigned sending_enabled;
} eventStorage_t;

int eventStorage_init(eventStorage_t *e,
                      eventBufferDesc_t *buffers,
                      unsigned num_of_buffers,
                      unsigned default_buffer);

int eventStorage_dispatch(eventStorage_t *e, const event_t *event);

int eventStorage_update(eventStorage_t *e);

static inline void eventStorage_setPolicy(eventStorage_t *e,
                                          uint8_t event_type,
                                          unsigned buffer)
{
    e->dispatch_policy[event_type] = buffer;
}

unsigned eventStorage_getNumOfEvents(eventStorage_t *e);

static inline void eventStorage_disableSending(eventStorage_t *e)
{
    e->sending_enabled = 0;
}

static inline void eventStorage_enableSending(eventStorage_t *e)
{
    e->sending_enabled = 1;
}

static inline unsigned eventStorage_isSendingEnabled(eventStorage_t *e)
{
    return e->sending_enabled;
}

#endif /* EVENT_STORAGE_H_ */
