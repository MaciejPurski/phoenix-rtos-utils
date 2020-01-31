/*
 * GPIO driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP_MON_HAL_GPIO_H
#define MSP_MON_HAL_GPIO_H

#include <hal.h>

#define GPIO_RES__OK                            (0)
#define GPIO_RES__INVALID_PORT                  (-1)
#define GPIO_RES__ARG_ERROR                     (-2)

typedef enum {
    gpio_mode__in,
    gpio_mode__out,
    gpio_mode__alt, /* Alternative function */
} gpio_mode_t;

typedef enum {
    gpio_pull__none,
    gpio_pull__up,
    gpio_pull__down,
} gpio_pull_t;

/* Drive strength */
typedef enum {
    gpio_strength__high,
    gpio_strength__low,
} gpio_strength_t;

int gpio_init(int port, int pin, gpio_mode_t mode, gpio_pull_t pull);
int gpio_read(int port, int pin);
int gpio_write(int port, int pin, int state);
int gpio_toggle(int port, int pin);
int gpio_setDriveStrength(int port, int pin, gpio_strength_t s);

#endif /* MSP_MON_HAL_GPIO_H */
