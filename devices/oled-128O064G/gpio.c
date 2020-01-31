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


#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "gpio.h"


int gpio_configMux(int mux, int sion, int mode)
{
	platformctl_t pctl;

	pctl.action = pctl_set;
	pctl.type = pctl_iomux;

	pctl.iomux.mux = mux;
	pctl.iomux.sion = sion;
	pctl.iomux.mode = mode;

	return platformctl(&pctl);
}


int gpio_configPad(int pad, int hys, int pus, int pue, int pke, int ode, int speed, int dse, int sre)
{
	platformctl_t pctl;

	pctl.action = pctl_set;
	pctl.type = pctl_iopad;

	pctl.iopad.pad = pad;
	pctl.iopad.hys = hys;
	pctl.iopad.pus = pus;
	pctl.iopad.pue = pue;
	pctl.iopad.pke = pke;
	pctl.iopad.ode = ode;
	pctl.iopad.speed = speed;
	pctl.iopad.dse = dse;
	pctl.iopad.sre = sre;

	return platformctl(&pctl);
}


void gpio_setDir(int gpiofd, int pin, int dir)
{
	gpiodata_t data;

	data.w.val = !!dir << pin;
	data.w.mask = 1UL << pin;

	write(gpiofd, &data, sizeof(data));
}


void gpio_setPin(int gpiofd, int pin, int state)
{
	gpiodata_t data;

	data.w.val = !!state << pin;
	data.w.mask = 1UL << pin;

	write(gpiofd, &data, sizeof(data));
}


int gpio_getPin(int gpiofd, int pin)
{
	unsigned int val;

	read(gpiofd, &val, sizeof(val));

	return !!(val & (1UL << pin));
}


void gpio_setPort(int gpiofd, unsigned int state, unsigned int mask)
{
	gpiodata_t data;

	data.w.val = state;
	data.w.mask = mask;

	write(gpiofd, &data, sizeof(data));
}


int gpio_openPort(int gpio)
{
	int fd;
	char path[] = "/dev/gpiox/port";

	if (gpio < gpio1 || gpio > gpio5)
		return -1;

	path[9] = '1' + gpio;

	while ((fd = open(path, O_RDWR)) < 0)
		usleep(1000);

	return fd;
}


int gpio_openDir(int gpio)
{
	int fd;
	char path[] = "/dev/gpiox/dir";

	if (gpio < gpio1 || gpio > gpio5)
		return -1;

	path[9] = '1' + gpio;

	while ((fd = open(path, O_RDWR)) < 0)
		usleep(1000);

	return fd;
}
