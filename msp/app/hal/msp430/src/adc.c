/*
 * ADC driver
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <adc.h>

void adc_init(void)
{
    ADC10CTL0 = ADC10ON | ADC10SHT_3;
    ADC10CTL1 = ADC10CONSEQ_0 | ADC10SSEL_1 | ADC10DIV_0 | ADC10SHP;
    ADC10CTL2 = ADC10SR | ADC10RES | ADC10RES | ADC10PDIV__1;

    /* Enable temperature sensor */
    REFCTL0 |= REFON;
}

uint16_t adc_conversion(unsigned channel)
{
    ADC10MCTL0 &= ~(ADC10INCH0 | ADC10INCH1 | ADC10INCH2 | ADC10INCH3);
    ADC10MCTL0 |= channel;

    /* Enable */
    ADC10CTL0 |= ADC10ENC;

    /* Trigger conversion */
    ADC10CTL0 |= ADC10SC;

    /* Wait until conversion is done */
    while (!(ADC10IFG & ADC10IFG0));

    /* Disable */
    ADC10CTL0 &= ~ADC10ENC;

    return ADC10MEM0;
}

void adc_deinit(void)
{
    /* Disable temperature sensor */
    REFCTL0 &= ~REFON;

    ADC10CTL0 &= ~ADC10ON;
}
