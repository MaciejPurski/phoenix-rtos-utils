/*
 * Phoenix-RTOS
 *
 * OLED physical interface
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include <unistd.h>

#include "gpio.h"


enum { lcd_d0 = 0, lcd_d1, lcd_d2, lcd_d3, lcd_d4, lcd_d5, lcd_d6, lcd_d7, lcd_e_rd, lcd_r_w,
	lcd_cs, lcd_bs1, lcd_bs2, lcd_d_c, lcd_res, lcd_pwr, lcd_key0, lcd_key1, lcd_total };


enum { lcd_sclk = lcd_d0, lcd_sdin = lcd_d1 };


/*
 * OLED PINS VS CPU PINS:
 *
 * LCD_D0 -> LCD_RESET
 * LCD_D1 -> LCD_HSYNC
 * LCD_D2 -> LCD_ENABLE
 * LCD_D3 -> LCD_CLK
 * LCD_D4 -> LCD_DATA00
 * LCD_D5 -> LCD_DATA01
 * LCD_D6 -> LCD_DATA02
 * LCD_D7 -> LCD_DATA03
 * LCD_E/RD -> LCD_DATA04
 * LCD_R/W -> LCD_DATA05
 * LCD_CS -> LCD_DATA06
 * LCD_BS1 -> LCD_DATA07
 * LCD_BS2 -> LCD_DATA08
 * LCD_D/C -> LCD_DATA16
 * LCD_RES -> SNVS_TAMPER0
 * LCD_PWR_CTRL -> SNVS_TAMPER1
 *
 * KEY_0 -> LCD_DATA12
 * KEY_1 -> LCD_DATA15
*/


static const struct {
	int mux;
	int pad;
	int gpio;
	int pin;
	int dir;
	int state;
} pininfo[lcd_total] = {
	{ pctl_mux_lcd_rst, pctl_pad_lcd_rst, gpio3, 4, output, low },      /* lcd_d0 (spi_sclk) */
	{ pctl_mux_lcd_hsync, pctl_pad_lcd_hsync, gpio3, 2, output, low },  /* lcd_d1 (spi_sdin) */
	{ pctl_mux_lcd_en, pctl_pad_lcd_en, gpio3, 1, output, low },        /* lcd_d2 */
	{ pctl_mux_lcd_clk, pctl_pad_lcd_clk, gpio3, 0, output, low },      /* lcd_d3 */
	{ pctl_mux_lcd_d0, pctl_pad_lcd_d0, gpio3, 5, output, low },        /* lcd_d4 */
	{ pctl_mux_lcd_d1, pctl_pad_lcd_d1, gpio3, 6, output, low },        /* lcd_d5 */
	{ pctl_mux_lcd_d2, pctl_pad_lcd_d2, gpio3, 7, output, low },        /* lcd_d6 */
	{ pctl_mux_lcd_d3, pctl_pad_lcd_d3, gpio3, 8, output, low },        /* lcd_d7 */
	{ pctl_mux_lcd_d4, pctl_pad_lcd_d4, gpio3, 9, output, high },       /* lcd_e_rd */
	{ pctl_mux_lcd_d5, pctl_pad_lcd_d5, gpio3, 10, output, low },       /* lcd_r_w */
	{ pctl_mux_lcd_d6, pctl_pad_lcd_d6, gpio3, 11, output, high},       /* lcd_cs (spi_cs) */
	{ pctl_mux_lcd_d7, pctl_pad_lcd_d7, gpio3, 12, output, high },      /* lcd_bs1 */
	{ pctl_mux_lcd_d8, pctl_pad_lcd_d8, gpio3, 13, output, high },      /* lcd_bs2 */
	{ pctl_mux_lcd_d16, pctl_pad_lcd_d16, gpio3, 21, output, low },     /* lcd_d_c (spi_dc) */
	{ pctl_mux_tamper0, pctl_pad_tamper0, gpio5, 0, output, low },      /* lcd_res */
	{ pctl_mux_tamper1, pctl_pad_tamper1, gpio5, 1, output, low } };    /* lcd_pwr */


struct {
	int gpio3port;
	int gpio3dir;
	int gpio5port;
	int gpio5dir;
} oledphy_common;


static inline int gpio2fd(int gpio)
{
	switch (gpio) {
		case gpio3:
			return oledphy_common.gpio3port;
		case gpio5:
			return oledphy_common.gpio5port;
	}

	return -1;
}


static inline int dir2fd(int dir)
{
	switch (dir) {
		case gpio3:
			return oledphy_common.gpio3dir;
		case gpio5:
			return oledphy_common.gpio5dir;
	}

	return -1;
}


static void putByte(unsigned char byte)
{
	int i;
	unsigned int state = 0;
	const unsigned int mask = (1UL << pininfo[lcd_d0].pin) | (1UL << pininfo[lcd_d1].pin) |
							  (1UL << pininfo[lcd_d2].pin) | (1UL << pininfo[lcd_d3].pin) |
							  (1UL << pininfo[lcd_d4].pin) | (1UL << pininfo[lcd_d5].pin) |
							  (1UL << pininfo[lcd_d6].pin) | (1UL << pininfo[lcd_d7].pin);

	for (i = 0; i < 8; ++i)
		state |= !!(byte & (1 << i)) << pininfo[lcd_d0 + i].pin;

	/* We assume that all pins of data are on the same port */
	gpio_setPort(gpio2fd(pininfo[lcd_d0].gpio), state, mask);
}


void oledphy_sendCmd(unsigned char cmd)
{
	gpio_setPin(gpio2fd(pininfo[lcd_d_c].gpio), pininfo[lcd_d_c].pin, low);
	putByte(cmd);
	gpio_setPin(gpio2fd(pininfo[lcd_cs].gpio), pininfo[lcd_cs].pin, low);
	gpio_setPin(gpio2fd(pininfo[lcd_cs].gpio), pininfo[lcd_cs].pin, high);
}


void oledphy_sendData(unsigned char data)
{
	gpio_setPin(gpio2fd(pininfo[lcd_d_c].gpio), pininfo[lcd_d_c].pin, high);
	putByte(data);
	gpio_setPin(gpio2fd(pininfo[lcd_cs].gpio), pininfo[lcd_cs].pin, low);
	gpio_setPin(gpio2fd(pininfo[lcd_cs].gpio), pininfo[lcd_cs].pin, high);
}


void oledphy_init(void)
{
	int i;

	oledphy_common.gpio3port = gpio_openPort(gpio3);
	oledphy_common.gpio5port = gpio_openPort(gpio5);
	oledphy_common.gpio3dir = gpio_openDir(gpio3);
	oledphy_common.gpio5dir = gpio_openDir(gpio5);

	for (i = 0; i < lcd_total; ++i) {
		gpio_setPin(gpio2fd(pininfo[i].gpio), pininfo[i].pin, pininfo[i].state);
		gpio_setDir(dir2fd(pininfo[i].gpio), pininfo[i].pin, pininfo[i].dir);
		gpio_configMux(pininfo[i].mux, 0, 5);
		if (pininfo[i].dir == output)
			gpio_configPad(pininfo[i].pad, 0, 0, 0, 0, 0, 2, 4, 0);
		else
			gpio_configPad(pininfo[i].pad, 0, 1, 1, 1, 0, 2, 0, 0);
	}

	gpio_setPin(gpio2fd(pininfo[lcd_res].gpio), pininfo[lcd_res].pin, low);
	usleep(1000);
	gpio_setPin(gpio2fd(pininfo[lcd_res].gpio), pininfo[lcd_res].pin, high);;
	usleep(1000);
	gpio_setPin(gpio2fd(pininfo[lcd_pwr].gpio), pininfo[lcd_pwr].pin, high);
}
