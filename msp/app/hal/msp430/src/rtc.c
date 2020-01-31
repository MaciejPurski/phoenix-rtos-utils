/*
 * MSP430 RTC driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <hal.h>
#include <rtc.h>
#include <stdlib.h>

volatile uint16_t rtc_systime = 0;

int rtc_init(void)
{
    RTCCTL01 = RTCSSEL__RT1PS;
    RTCPS0CTL = RT0PSDIV_7;
    RTCPS1CTL = RT1SSEL_2 | RT1PSDIV_6 | RT1IP_6 | RT1PSIE;

    rtc_setUnixTime(0);

    return 0;
}

int rtc_getUnixTime(uint32_t *time)
{
    if (time == NULL)
        return -1;

    *time = RTCNT1 | ((uint32_t)RTCNT2 << 8) | ((uint32_t)RTCNT3 << 16) | ((uint32_t)RTCNT4 << 24);

    return 0;
}

int rtc_setUnixTime(uint32_t time)
{
    (void)time;

    RTCNT1 = time & 0xff;
    RTCNT2 = (time >> 8) & 0xff;
    RTCNT3 = (time >> 16) & 0xff;
    RTCNT4 = (time >> 24) & 0xff;

    return 0;
}

uint16_t rtc_getSysTime(void)
{
    return rtc_systime;
}

#define FREQ_HZ ((uint32_t)32768)

void rtc_sleepMs(uint16_t ms)
{
    uint16_t start = RTCPS;
    uint16_t ticks = ((ms * FREQ_HZ) + 1000 - 1)/1000 + 1;

    while ((RTCPS - start) < ticks)
        _no_operation();
}

void __attribute__((interrupt(RTC_VECTOR))) __interrupt_vector_rtc(void)
{
    rtc_systime++;
    RTCPS1CTL &= ~RT1PSIFG;

    /* Exit low power mode (LPM3) */
    __bic_SR_register_on_exit(LPM3_bits);
}

