/*
 * RAM event buffer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef EVENT_BUFFER_RAM_H_
#define EVENT_BUFFER_RAM_H_

#include "event_storage.h"

#include <stddef.h>

typedef struct {
    event_t *mem;
    size_t size;

    unsigned first;
    unsigned last;
    unsigned full;

    unsigned lastId; /* ID of the most recent element */
} eventBufferRAM_t;

int eventBufferRAM_init(void *buffer);
int eventBufferRAM_getNumOfEvents(void *buffer);
int eventBufferRAM_push(void *buffer, const event_t *event);
int eventBufferRAM_peak(void *buffer, event_t *event, unsigned *id);
int eventBufferRAM_remove(void *buffer, unsigned id);
void eventBufferRAM_initBufferDesc(void *buffer, eventBufferDesc_t *desc);

#endif /* EVENT_BUFFER_RAM_H_ */
