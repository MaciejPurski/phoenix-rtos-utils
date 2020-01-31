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

#ifndef _OLED_FUNCTION_ETH_H_
#define _OLED_FUNCTION_ETH_H_

void oledfun_get_en1_ip(char *dst, int exec);


void oledfun_get_en1_mask(char *dst, int exec);


void oledfun_get_en1_status(char *dst, int exec);


void oledfun_get_en1_link(char *dst, int exec);


void oledfun_get_en1_type(char *dst, int exec);


void oledfun_get_en2_ip(char *dst, int exec);


void oledfun_get_en2_mask(char *dst, int exec);


void oledfun_get_en2_status(char *dst, int exec);


void oledfun_get_en2_link(char *dst, int exec);


void oledfun_get_en2_type(char *dst, int exec);


void oledfun_get_gsm_ifname(char *dst, int exec);


void oledfun_get_gsm_status(char *dst, int exec);


void oledfun_get_gsm_media(char *dst, int exec);


void oledfun_get_gsm_ip(char *dst, int exec);


void oledfun_get_gsm_apn(char *dst, int exec);


void oledfun_get_gsm_link(char *dst, int exec);


void oledfun_get_vpn_status(char *dst, int exec);


void oledfun_get_vpn_ifname(char *dst, int exec);


void oledfun_get_vpn_ip(char *dst, int exec);


void oledfun_get_vpn_link(char *dst, int exec);


#endif
