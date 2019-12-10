/*
 * Phoenix-RTOS
 *
 * i.MX RT AD7779 driver.
 *
 * Copyright 2018, 2019 Phoenix Systems
 * Author: Krystian Wasik, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef AD7779_H
#define AD7779_H

#include <stdint.h>

#define AD7779_OK                       (0)
#define AD7779_ARG_ERROR                (-1)
#define AD7779_CTRL_IO_ERROR            (-2)
#define AD7779_CTRL_HEADER_ERROR        (-3)
#define AD7779_GPIO_INIT_ERROR          (-4)
#define AD7779_GPIO_IO_ERROR            (-5)
#define AD7779_VERIFY_FAILED            (-6)

#define AD7779_NUM_OF_CHANNELS          (8)
#define AD7779_NUM_OF_BITS              (24)

typedef enum {
	ad7779_mode__low_power,
	ad7779_mode__high_resolution,
} ad7779_mode_t;

int ad7779_init(void);

int ad7779_get_mode(ad7779_mode_t *mode);
int ad7779_set_mode(ad7779_mode_t mode);

int ad7779_get_sampling_rate(uint32_t *fs);
int ad7779_set_sampling_rate(uint32_t fs);

int ad7779_get_channel_gain(uint8_t channel, uint8_t *gain);
int ad7779_set_channel_gain(uint8_t channel, uint8_t gain);

/* For debugging purposes */
int ad7779_print_status(void);

#endif /* AD7779_H */
