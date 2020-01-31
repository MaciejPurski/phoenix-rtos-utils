/*
 * MSP430 hardware abstraction layer
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <hal.h>
#include <chip.h>
#include <gpio.h>
#include <stddef.h>

#define DCO_SPEED 4000000
#define ACLK_SPEED 32768

#define USE_EXTERNAL_XT1

/* Magic bytes must be added when the device is equipped with the custom BSL
 * expecting them, while the application is flashed using the programmer. */
#if 0
const uint8_t  __attribute__((section(".magic_bytes"))) magic_bytes[] = {0xde, 0xad, 0xbe, 0xef};
#endif

int hal_init(void)
{
    /* Stop watchdog */
    WDTCTL = WDTPW | WDTHOLD;

#ifdef USE_EXTERNAL_XT1
    /* Configure XIN and XOUT so that we can use external clock source */
    gpio_init(5, 4, gpio_mode__alt, gpio_pull__none);
    gpio_init(5, 5, gpio_mode__alt, gpio_pull__none);
#endif

    /* Use XT1 bypass since we're using TCXO instead of crystal */
    UCSCTL6 = XT1BYPASS;

    /* Configure Unified Clock System */
#ifdef USE_EXTERNAL_XT1
    UCSCTL4 = SELA__XT1CLK + SELM__DCOCLK + SELS__DCOCLK;
#else
    UCSCTL4 = SELA__REFOCLK + SELM__DCOCLK + SELS__DCOCLK;
#endif
    UCSCTL0 = 0x000;
    UCSCTL1 = DCORSEL_4;
    UCSCTL5 = DIVPA_0 + DIVA_0 + DIVM_0 + DIVS_0;
    UCSCTL2 = FLLD_2 | (((DCO_SPEED/ACLK_SPEED)/4) - 1);
    UCSCTL8 &= ~(SMCLKREQEN | MCLKREQEN);

    // Disable SVS
    PMMCTL0_H = PMMPW_H;                      // PMM Password
    SVSMHCTL &= ~(SVMHE+SVSHE);               // Disable High side SVS
    SVSMLCTL &= ~(SVMLE+SVSLE);               // Disable Low side SVS

    _no_operation();
    _enable_interrupts();

    return 0;
}

#define BSL_ENTRY_LOCATION      ((void(*)(void))0x1000)

void hal_enterBootloader(void)
{
    void (*bsl_entry)(void) = BSL_ENTRY_LOCATION;

    __disable_interrupt();

    bsl_entry();
}

void hal_enterStandbyMode(void)
{
    /* Enter low power mode (LPM3) */
    __bis_SR_register(LPM3_bits + GIE);
}

int hal_getResetReason(void)
{
    return SYSRSTIV;
}

const char *hal_getResetReasonAsString(void)
{
    int reason = hal_getResetReason();

    if (reason == 0)
        return NULL;

    switch (reason) {
    case 0x02: return "brownout";
    case 0x04: return "RST/NMI";
    case 0x06: return "PMMSWBOR";
    case 0x08: return "wakeup from LPMx.5";
    case 0x0A: return "security violation";
    case 0x0C: return "SVSL";
    case 0x0E: return "SVSH";
    case 0x10: return "SVML_OVP";
    case 0x12: return "SVMH_OVP";
    case 0x14: return "PMMSWPOR";
    case 0x16: return "WDT time out";
    case 0x18: return "WDT password violation";
    case 0x1A: return "flash password violation";
    case 0x1E: return "PERF";
    case 0x20: return "PMM password violation";
    default:   return "unrecognized";
    }
}

int hal_clock32kHzFault(void)
{
    /* Clear fault flags (flags that are still valid will stay set) */
    UCSCTL7 = 0;

    return UCSCTL7 & XT1LFOFFG;
}
