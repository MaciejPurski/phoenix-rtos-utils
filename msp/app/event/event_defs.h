/*
 * Event definitions and types
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef EVENT_DEFS_H_
#define EVENT_DEFS_H_

#include <stdint.h>
#include <string.h>

#define EVENT_NONE              (0)

/* Tampers */
#define EVENT_TAMPER_1_START    (1)
#define EVENT_TAMPER_1_STOP     (2)
#define EVENT_TAMPER_2_START    (3)
#define EVENT_TAMPER_2_STOP     (4)

/* Watchdog */
#define EVENT_IMX_WDG_RESET     (5)

/* Magnetometer */
#define EVENT_MAG_X_START       (6)
#define EVENT_MAG_X_STOP        (7)
#define EVENT_MAG_Y_START       (8)
#define EVENT_MAG_Y_STOP        (9)
#define EVENT_MAG_Z_START       (10)
#define EVENT_MAG_Z_STOP        (11)

/* Accelerometer */
#define EVENT_ACCEL_ORIENTATION (12)

/* Power */
#define EVENT_MAIN_POWER_OUTAGE (13)
#define EVENT_MAIN_POWER_BACK   (14)
#define EVENT_AUX_POWER_OUTAGE  (15)
#define EVENT_AUX_POWER_BACK    (16)
#define EVENT_BATTERY_LOW       (17)
#define EVENT_BATTERY_OK        (18)

/* Reset */
#define EVENT_MSP_RESET         (19)

#define NUM_OF_EVENT_TYPES      (20)

/* NOTE!
 * Must be aligned to 4 bytes to prevent illegal unaligned access on MSP430.
 * To preserved space on FRAM serialization by hand is used (this way
 * we're using 5 instead of 8 bytes per event. */
typedef struct __attribute__((packed, aligned(4))) {
    uint32_t timestamp;
    uint8_t type;
} event_t;

#define EVENT_SIZE_SERIALIZED       (sizeof(uint32_t) + sizeof(uint8_t))

static inline void event_serialize(const event_t *event, uint8_t *buffer)
{
    memcpy(buffer, &event->timestamp, sizeof(uint32_t));
    memcpy(buffer + sizeof(uint32_t), &event->type, sizeof(uint8_t));
}

static inline void event_deserialize(event_t *event, const uint8_t *buffer)
{
    memcpy(&event->timestamp, buffer, sizeof(uint32_t));
    memcpy(&event->type, buffer + sizeof(uint32_t), sizeof(uint8_t));
}

#endif /* EVENT_DEFS_H_ */
