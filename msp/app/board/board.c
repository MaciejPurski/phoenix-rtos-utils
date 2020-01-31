/*
 * board.c
 *
 *  Created on: Jul 26, 2018
 *      Author: phoenix
 */

#include <board.h>

#include <gpio.h>
#include <spi.h>
#include <serial.h>
#include <adc.h>
#include <rtc.h>

#include <lis3mdl.h>
#include <lis2dh12.h>

void board_init(void)
{
    board_resetPinConfig();

    /* Refresh external watchdog right away */
    gpio_toggle(EXT_WDG_PORT, EXT_WDG_PIN);
}

void board_resetPinConfig(void)
{
#if 0
    P1OUT = 0x00; P2OUT = 0x00; P3OUT = 0x00; P4OUT = 0x00; P5OUT = 0x00; P6OUT = 0x00;
    PJOUT = 0x00;
    P1DIR = 0xFF; P2DIR = 0xFF; P3DIR = 0xFF; P4DIR = 0xFF; P5DIR = 0xFF; P6DIR = 0xFF;
    PJDIR = 0xFF;

    /* VPRI, VSEC, VBAT */
    gpio_init(6, 0, gpio_mode__in, gpio_pull__none);
    gpio_init(6, 1, gpio_mode__in, gpio_pull__none);
    gpio_init(6, 2, gpio_mode__in, gpio_pull__none);

    /* WDI */
    gpio_init(EXT_WDG_PORT, EXT_WDG_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(EXT_WDG_PORT, EXT_WDG_PIN, 0);

    /* TAMPER_1 and TAMPER_2 */
    gpio_init(TAMPER1_PORT, TAMPER1_PIN, gpio_mode__in, gpio_pull__up);
    gpio_init(TAMPER2_PORT, TAMPER2_PIN, gpio_mode__in, gpio_pull__up);

    /* A0_RXD and A0_TXD */
    gpio_init(3, 4, gpio_mode__in, gpio_pull__up);
    gpio_init(3, 3, gpio_mode__out, gpio_pull__none);
    gpio_write(3, 3, 0);

    /* B0_SIMO, B0_SOMI, B0_SCLK */
    gpio_init(3, 0, gpio_mode__out, gpio_pull__none);
    gpio_write(3, 0, 1);
    gpio_init(3, 1, gpio_mode__in, gpio_pull__up);
    gpio_init(3, 2, gpio_mode__out, gpio_pull__none);
    gpio_write(3, 2, 1);

    /* FRAM (power up, but keep CS high) */
    gpio_init(FM25L04B_PWRD_PORT, FM25L04B_PWRD_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(FM25L04B_PWRD_PORT, FM25L04B_PWRD_PIN, 1);
    gpio_init(FM25L04B_CS_PORT, FM25L04B_CS_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(FM25L04B_CS_PORT, FM25L04B_CS_PIN, 1);

    /* PFO */
    gpio_init(PFO_PORT, PFO_PIN, gpio_mode__in, gpio_pull__down);

    /* MRST, MCNTR */
    gpio_init(IMX_RESET_PORT, IMX_RESET_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(IMX_RESET_PORT, IMX_RESET_PIN, 0);
    gpio_init(IMX_ONOFF_PORT, IMX_ONOFF_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(IMX_ONOFF_PORT, IMX_ONOFF_PIN, 0);

    /* Keep chip selects high */
    gpio_init(LIS2DH12_CS_PORT, LIS2DH12_CS_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(LIS2DH12_CS_PORT, LIS2DH12_CS_PIN, 1);
    gpio_init(LIS3MDL_CS_PORT, LIS3MDL_CS_PIN, gpio_mode__out, gpio_pull__none);
    gpio_write(LIS3MDL_CS_PORT, LIS3MDL_CS_PIN, 1);
#else
    /* Optimized version of the above */
    PAOUT = 0x0030; PADIR = 0xFFFE;
    PBOUT = 0x4817; PBDIR = 0xFF0D;
    PCOUT = 0x000C; PCDIR = 0xF833;
    PJOUT = 0x0000; PJDIR = 0x000F;
#endif
}

#define VBAT_MULT               (1487)
#define VBAT_DIV                (100)

uint32_t board_getBatteryVoltage(void)
{
    /* Voltage in mV */
    return adc_conversion(VBAT_ADC_CHANNEL) * (uint32_t)VBAT_MULT / VBAT_DIV;
}

#define VPRI_MULT               (4137)
#define VPRI_DIV                (100)

uint32_t board_getPrimaryVoltage(void)
{
    /* Voltage in mV */
    return adc_conversion(VPRI_ADC_CHANNEL) * (uint32_t)VPRI_MULT / VPRI_DIV;
}

#define VSEC_MULT               (4137)
#define VSEC_DIV                (100)

uint32_t board_getSecondaryVoltage(void)
{
    /* Voltage in mV */
    return adc_conversion(VSEC_ADC_CHANNEL) * (uint32_t)VSEC_MULT / VSEC_DIV;
}

#define TEMP_MULT               (3 * (uint32_t)396825)
#define TEMP_DIV                (1024)
#define TEMP_SHIFT              (273000)

#define TEMP_INVALID            ((int16_t)-274)

int32_t board_getTemperature(int idx)
{
    /* Temperature in Celsius degrees * 10^(-3)  */
    int32_t temp;

    if (idx == 0) {
        temp = adc_conversion(TEMP_ADC_CHANNEL) * (uint32_t)TEMP_MULT / TEMP_DIV - TEMP_SHIFT;
    } else if (idx == 1) {
        lis3mdl_readTemp(&temp);
    } else {
        temp = TEMP_INVALID;
    }

    return temp;
}
