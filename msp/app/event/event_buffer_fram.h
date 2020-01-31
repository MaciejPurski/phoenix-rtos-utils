/*
 * FRAM event buffer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef EVENT_BUFFER_FRAM_H_
#define EVENT_BUFFER_FRAM_H_

#include "event_storage.h"

typedef struct {
    uint16_t fram_addr;
    size_t size;

    unsigned first;
    unsigned last;
    unsigned full;

    unsigned lastId; /* ID of the most recent element */
} eventBufferFRAM_t;

int eventBufferFRAM_init(void *buffer);
int eventBufferFRAM_getNumOfEvents(void *buffer);
int eventBufferFRAM_push(void *buffer, const event_t *event);
int eventBufferFRAM_peak(void *buffer, event_t *event, unsigned *id);
int eventBufferFRAM_remove(void *buffer, unsigned id);
void eventBufferFRAM_initBufferDesc(void *buffer, eventBufferDesc_t *desc);

#endif /* EVENT_BUFFER_FRAM_H_ */
