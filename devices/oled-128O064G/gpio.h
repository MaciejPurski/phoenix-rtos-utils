/*
 * Phoenix-RTOS
 *
 * GPIO interface
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _OLED_GPIO_H_
#define _OLED_GPIO_H_


#include <phoenix/arch/imx6ull.h>
#include "../../phoenix-rtos-devices/gpio/imx6ull-gpio/imx6ull-gpio.h"
#include <sys/platform.h>


enum { gpio1 = 0, gpio2, gpio3, gpio4, gpio5 };


enum { input = 0, output };


enum { low = 0, high };


int gpio_configMux(int mux, int sion, int mode);


int gpio_configPad(int pad, int hys, int pus, int pue, int pke, int ode, int speed, int dse, int sre);


void gpio_setDir(int gpiofd, int pin, int dir);


void gpio_setPin(int gpiofd, int pin, int state);


int gpio_getPin(int gpiofd, int pin);


void gpio_setPort(int gpiofd, unsigned int state, unsigned int mask);


int gpio_openPort(int gpio);


int gpio_openDir(int gpio);


#endif
