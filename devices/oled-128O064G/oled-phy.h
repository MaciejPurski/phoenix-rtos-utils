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


#ifndef _OLED_PHY_H_
#define _OLED_PHY_H_


void oledphy_sendCmd(unsigned char cmd);


void oledphy_sendData(unsigned char data);


void oledphy_init(void);


#endif
