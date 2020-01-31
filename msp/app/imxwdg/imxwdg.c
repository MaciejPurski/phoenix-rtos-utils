/*
 * imxwdg.c
 *
 *  Created on: Jul 20, 2018
 *      Author: phoenix
 */

#include <gpio.h>
#include <rtc.h>

#include <imxwdg.h>

#include <event.h>

uint16_t _last_update = 0;
uint16_t _min_refresh_rate;

static inline void imxwdg_reset(void)
{
    gpio_write(IMX_RESET_PORT, IMX_RESET_PIN, 1);

    for (unsigned i = 20000; i > 0; i--)
        asm("nop");

    gpio_write(IMX_RESET_PORT, IMX_RESET_PIN, 0);

    event_addNow(EVENT_IMX_WDG_RESET);
}

void imxwdg_init(void)
{
    _min_refresh_rate = IMXWDG_MIN_REFRESH_RATE;

    gpio_init(IMX_RESET_PORT, IMX_RESET_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(IMX_RESET_PORT, IMX_RESET_PIN, 0);

    imxwdg_refresh();
}

void imxwdg_refresh(void)
{
    _last_update = rtc_getSysTime();
}

int imxwdg_update(void)
{
    if (rtc_getSysTime() - _last_update > _min_refresh_rate) {
        imxwdg_reset();
        imxwdg_refresh();
        return 1;
    }

    return 0;
}
