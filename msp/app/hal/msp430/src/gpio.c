/*
 * MSP430 GPIO driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <gpio.h>
#include <stdlib.h>

typedef struct {
    uint8_t in;
    uint8_t reserved0;
    uint8_t out;
    uint8_t reserved1;
    uint8_t dir;
    uint8_t reserved2;
    uint8_t ren;
    uint8_t reserved3;
    uint8_t ds;
    uint8_t reserved4;
    uint8_t sel;
} __attribute__((packed, aligned(1))) gpio_regs_t;

#define NUM_OF_GPIO_INSTANCES                   (7)

static gpio_regs_t * const gpio[NUM_OF_GPIO_INSTANCES] = {
    (gpio_regs_t*)NULL, /* No port zero */
    (gpio_regs_t*)&P1IN,
    (gpio_regs_t*)&P2IN,
    (gpio_regs_t*)&P3IN,
    (gpio_regs_t*)&P4IN,
    (gpio_regs_t*)&P5IN,
    (gpio_regs_t*)&P6IN,
};

int gpio_init(int port, int pin, gpio_mode_t mode, gpio_pull_t pull)
{
    if (port >= NUM_OF_GPIO_INSTANCES || gpio[port] == NULL)
        return GPIO_RES__INVALID_PORT;

    uint8_t mask = 1 << pin;

    if (mode == gpio_mode__alt) {
        gpio[port]->sel |= mask;

    } else if (mode == gpio_mode__in) {
        gpio[port]->sel &= ~mask;
        gpio[port]->dir &= ~mask;

        if (pull == gpio_pull__none) {
            gpio[port]->ren &= ~mask;

        } else if (pull == gpio_pull__down) {
            gpio[port]->ren |= mask;
            gpio[port]->out &= ~mask;

        } else if (pull == gpio_pull__up) {
            gpio[port]->ren |= mask;
            gpio[port]->out |= mask;
        }

    } else if (mode == gpio_mode__out) {
        gpio[port]->sel &= ~mask;
        gpio[port]->dir |= mask;

    } else {
        return GPIO_RES__ARG_ERROR;
    }

    return GPIO_RES__OK;
}

int gpio_read(int port, int pin)
{
    if (port >= NUM_OF_GPIO_INSTANCES || gpio[port] == NULL)
        return GPIO_RES__INVALID_PORT;

    if (gpio[port]->in & (1 << pin)) {
        return 1;
    } else {
        return 0;
    }
}

int gpio_write(int port, int pin, int state)
{
    if (port >= NUM_OF_GPIO_INSTANCES || gpio[port] == NULL)
        return GPIO_RES__INVALID_PORT;

    if (state) {
        gpio[port]->out |= (1 << pin);
    } else {
        gpio[port]->out &= ~(1 << pin);
    }

    return GPIO_RES__OK;
}

int gpio_toggle(int port, int pin)
{
    if (port >= NUM_OF_GPIO_INSTANCES || gpio[port] == NULL)
        return GPIO_RES__INVALID_PORT;

    gpio[port]->out ^= 1 << pin;

    return GPIO_RES__OK;
}

int gpio_setDriveStrength(int port, int pin, gpio_strength_t s)
{
    if (port >= NUM_OF_GPIO_INSTANCES || gpio[port] == NULL)
        return GPIO_RES__INVALID_PORT;

    if (s == gpio_strength__high) {
        gpio[port]->ds |= 1 << pin;
    } else {
        gpio[port]->ds &= ~(1 << pin);
    }

    return GPIO_RES__OK;
}
