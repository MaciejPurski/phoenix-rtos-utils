/*
 * Event subsystem
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "event_storage.h"

#include "event_buffer_ram.h"
#include "event_buffer_fram.h"

#include <log.h>

#ifdef EVENT_HAS_RTC
    #include <rtc.h>
#endif /* EVENT_HAS_RTC */

#define GENERAL_LOG             (0)
#define POWER_FAULT_LOG         (1)
#define TAMPER_LOG              (2)
#define MOVEMENT_LOG            (3)

#define NUM_OF_EVENT_LOGS       (4)

#define GENERAL_LOG_SIZE        (32)
#define POWER_FAULT_LOG_SIZE    (32)
#define TAMPER_LOG_SIZE         (34)
#define MOVEMENT_LOG_SIZE       (34)

static event_t generalLogMem[GENERAL_LOG_SIZE];

static eventBufferFRAM_t powerFaultLog  = {.fram_addr = 0x00a,      .size = POWER_FAULT_LOG_SIZE};
static eventBufferFRAM_t tamperLog      = {.fram_addr = 0x0aa,      .size = TAMPER_LOG_SIZE};
static eventBufferFRAM_t movementLog    = {.fram_addr = 0x154,      .size = MOVEMENT_LOG_SIZE};
static eventBufferRAM_t generalLog      = {.mem = generalLogMem,    .size = GENERAL_LOG_SIZE};

static eventBufferDesc_t eventBuffers[NUM_OF_EVENT_LOGS];

static eventStorage_t eventStorage;

int event_init(int use_fram)
{
    int res;
    unsigned num_of_buffers;

    eventBufferRAM_initBufferDesc(&generalLog, &eventBuffers[GENERAL_LOG]);

    if (use_fram) {
        eventBufferFRAM_initBufferDesc(&powerFaultLog, &eventBuffers[POWER_FAULT_LOG]);
        eventBufferFRAM_initBufferDesc(&tamperLog, &eventBuffers[TAMPER_LOG]);
        eventBufferFRAM_initBufferDesc(&movementLog, &eventBuffers[MOVEMENT_LOG]);
        num_of_buffers = NUM_OF_EVENT_LOGS;
    } else {
        num_of_buffers = 1;
    }

    res = eventStorage_init(&eventStorage, eventBuffers, num_of_buffers, GENERAL_LOG);
    if (res < 0)
        return res;

    /* Set dispatch policy */
    if (use_fram) {
        eventStorage_setPolicy(&eventStorage, EVENT_TAMPER_1_START, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_TAMPER_1_STOP, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_TAMPER_2_START, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_TAMPER_2_STOP, TAMPER_LOG);

        eventStorage_setPolicy(&eventStorage, EVENT_IMX_WDG_RESET, POWER_FAULT_LOG);

        eventStorage_setPolicy(&eventStorage, EVENT_MAG_X_START, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_MAG_X_STOP, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_MAG_Y_START, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_MAG_Y_STOP, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_MAG_Z_START, TAMPER_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_MAG_Z_STOP, TAMPER_LOG);

        eventStorage_setPolicy(&eventStorage, EVENT_ACCEL_ORIENTATION, MOVEMENT_LOG);

        eventStorage_setPolicy(&eventStorage, EVENT_MAIN_POWER_OUTAGE, POWER_FAULT_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_MAIN_POWER_BACK, POWER_FAULT_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_AUX_POWER_OUTAGE, POWER_FAULT_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_AUX_POWER_BACK, POWER_FAULT_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_BATTERY_LOW, POWER_FAULT_LOG);
        eventStorage_setPolicy(&eventStorage, EVENT_BATTERY_OK, POWER_FAULT_LOG);

        eventStorage_setPolicy(&eventStorage, EVENT_MSP_RESET, POWER_FAULT_LOG);
    }

    return 0;
}

int event_add(const event_t *event)
{
    int res = eventStorage_dispatch(&eventStorage, event);
    if (res < 0)
        log_error("event: failed to add event (type=%u, res=%d)", event->type, res);

    return res;
}

#ifdef EVENT_HAS_RTC
int event_addNow(uint8_t type)
{
    event_t event;
    event.type = type;

    if (rtc_getUnixTime(&event.timestamp) < 0)
        return -1;

    return event_add(&event);
}
#endif /* EVENT_HAS_RTC */

int event_update(void)
{
    return eventStorage_update(&eventStorage);
}

unsigned event_getNumOfEvents(void)
{
    return eventStorage_getNumOfEvents(&eventStorage);
}

void event_enableSending(void)
{
    eventStorage_enableSending(&eventStorage);
}

void event_disableSending(void)
{
    eventStorage_disableSending(&eventStorage);
}

unsigned event_isSendingEnabled(void)
{
    return eventStorage_isSendingEnabled(&eventStorage);
}
