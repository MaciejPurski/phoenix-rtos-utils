/*
 * Event storage
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "event_storage.h"

#include <stddef.h>

#include <msp_mon_prot.h>

#ifdef EVENT_STORAGE_DEBUG
#ifndef EVENT_STORAGE_NO_STDIO
    #include <stdio.h>

    #define COL_RED     "\033[1;31m"
    #define COL_CYAN    "\033[1;36m"
    #define COL_YELLOW  "\033[1;33m"
    #define COL_NORMAL  "\033[0m"

    #define LOG_TAG "eventStorage: "
    #define log_debug(fmt, ...)     do { printf(LOG_TAG fmt "\n", ##__VA_ARGS__); } while (0)
    #define log_info(fmt, ...)      do { printf(COL_CYAN LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
    #define log_warn(fmt, ...)      do { printf(COL_YELLOW LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
    #define log_error(fmt, ...)     do { printf(COL_RED  LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
#else
    #include <log.h>
#endif
#else
    #define log_debug(...)
    #define log_info(...)
    #define log_error(...)
    #define log_warn(...)
#endif

int eventStorage_init(eventStorage_t *e,
                      eventBufferDesc_t *buffers,
                      unsigned num_of_buffers,
                      unsigned default_buffer)
{
    unsigned i;

    if (e == NULL)
        return -1;

    e->buffers = buffers;
    e->num_of_buffers = num_of_buffers;
    e->sending = 0;
    e->sending_enabled = 0;

    /* Initialize dispatch policy */
    for (i = 0; i < NUM_OF_EVENT_TYPES; i++) {
        e->dispatch_policy[i] = default_buffer;
    }

    /* Initialize buffers */
    for (i = 0; i < e->num_of_buffers; i++) {
        if (e->buffers[i].init == NULL)
            return -1;

        if (e->buffers[i].init(e->buffers[i].buffer) != 0) {
            log_error("init: buffer init failed");
            return -1;
        }
    }

    return 0;
}

int eventStorage_dispatch(eventStorage_t *e, const event_t *event)
{
    if (e == NULL || event == NULL)
        return -1;

    if (event->type >= NUM_OF_EVENT_TYPES) {
        log_error("dispatch: event type too high");
        return -1;
    }

    unsigned buffer_no = e->dispatch_policy[event->type];
    if (buffer_no >= e->num_of_buffers) {
        log_error("dispatch: buffer id too high");
        return -1;
    }

    if (e->buffers[buffer_no].push == NULL)
        return -1;

    return e->buffers[buffer_no].push(e->buffers[buffer_no].buffer, event);
}

static int eventStorage_sendClbk(int res, const uint8_t *data, uint16_t len, void *arg)
{
    eventStorage_t *e = (eventStorage_t*)arg;
    eventBufferDesc_t *bufferDesc;

    if (e == NULL)
        return -1;

    e->sending = 0;

    if (res == MMP_RES__OK) {
        bufferDesc = &e->buffers[e->current_buffer];
        if (bufferDesc->remove == NULL) {
            log_error("sendClbk: NULL pointer to buffer remove");
            return -1;
        }

        if (bufferDesc->remove(bufferDesc->buffer, e->current_id) < 0) {
            log_error("sendClbk: buffer remove failed");
            return -1;
        }
    } else if (res == MMP_RES__NACK) {
        log_error("sendClbk: received NACK. Disabling sending events");
        eventStorage_disableSending(e);
    } else if (res != MMP_RES__DENINITIALIZED) {
        log_error("sendClbk: failed (%d)", res);
        eventStorage_disableSending(e);
    }

    return 0;
}

int eventStorage_update(eventStorage_t *e)
{
    int res;
    event_t event;

    if (e == NULL)
        return -1;

    if (e->sending_enabled && !e->sending) {

        /* Check for events that need to be sent */
        unsigned i;
        for (i = 0; i < e->num_of_buffers; i++) {
            if (e->buffers[i].getNumOfEvents == NULL || e->buffers[i].peak == NULL)
                return -1;

            if (e->buffers[i].getNumOfEvents(e->buffers[i].buffer) > 0) {

                unsigned id;
                if (e->buffers[i].peak(e->buffers[i].buffer, &event, &id) < 0) {
                    log_error("update: buffer peak failed");
                    return -1;
                }

                res = mmp_transmit(mmp_get_default_instance(), MMP_CMD__PUSH_EVENT,
                    (uint8_t*)&event, sizeof(event_t), eventStorage_sendClbk, e, 0);

                if (res == MMP_RES__TX_BUSY || res == MMP_RES__UNINITIALIZED) {
                    break;
                } else if (res != MMP_RES__OK) {
                    log_error("update: mmp_transmit failed (%d)", res);
                    return -1;
                }

                e->sending = 1;
                e->current_buffer = i;
                e->current_id = id;

                break;
            }
        }
    }

    return 0;
}

unsigned eventStorage_getNumOfEvents(eventStorage_t *e)
{
    unsigned i, cnt = 0;
    for (i = 0; i < e->num_of_buffers; i++) {
        if (e->buffers[i].getNumOfEvents == NULL)
            return -1;
        cnt += e->buffers[i].getNumOfEvents(e->buffers[i].buffer);
    }
    return cnt;
}
