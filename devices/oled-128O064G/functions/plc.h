/*
 * Phoenix-RTOS
 *
 * Functions to be used by OLED-menu
 *
 * Copyright 2019 Phoenix Systems
 * Author: Andrzej Glowinski
 *
 * %LICENSE%
 */

#ifndef _OLED_FUNCTION_PLC_H_
#define _OLED_FUNCTION_PLC_H_

void oledfun_get_plc_snr(char *dst, int exec);


void oledfun_get_plc_txbs(char *dst, int exec);


void oledfun_get_plc_txds(char *dst, int exec);


void oledfun_get_plc_txbf(char *dst, int exec);


void oledfun_get_plc_txdf(char *dst, int exec);


void oledfun_get_plc_rx(char *dst, int exec);


#endif
