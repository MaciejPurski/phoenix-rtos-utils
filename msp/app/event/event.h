/*
 * Event subsystem
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef EVENT_H_
#define EVENT_H_

#include "event_defs.h"

int event_init(int use_fram);
int event_add(const event_t *event);
int event_update(void);
unsigned event_getNumOfEvents(void);

#ifdef EVENT_HAS_RTC
int event_addNow(uint8_t type);
#endif /* EVENT_HAS_RTC */

void event_enableSending(void);
void event_disableSending(void);
unsigned event_isSendingEnabled(void);

#endif /* EVENT_H_ */
