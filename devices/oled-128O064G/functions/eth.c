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

#include <string.h>
#include <stdio.h>

#include <arpa/inet.h>
#include <sys/time.h>

#include <ifaddrs.h>

#include "helper.h"

#include "eth.h"


#define IFSTATUS_PATH "/dev/ifstatus"
#define GSM_CFG_PATH "/local/etc/gsm.conf"

#define UPDATE_CACHE (2 * 1000 * 1000)

#define IF_NUM 4

typedef struct {
	uint8_t data_mask;
	char ifname[4];
	unsigned int flags;
	char ip[16];
	char mask[16];
	union {
		char broad[16];
		char dst[16];
		char brdstaddr[16];
	};
} eth_data_t;


static struct {
	eth_data_t en[IF_NUM];
	time_t last_update;
	uint8_t if_mask;
} eth_common;


enum {eth_ifname, eth_flags, eth_ip, eth_mask, eth_brdst};

enum {if_en1, if_en2, if_vpn, if_gsm};

static const char *if_strings[] = {"en1", "en2", "tu", "pp"};


static int find_in_config_file(const char *config, const char *key, char *value, int size)
{
	char buffer[1024];
	FILE *cfg_file = fopen(config, "r");
	int ret= -1;
	char *p;

	if (cfg_file == NULL)
		return -1;

	while (fgets(buffer, sizeof(buffer), cfg_file) != NULL) {
		if ((p = strstr(buffer, key)) != NULL) {
			while (*p && *p != '\n' && *p != '=')
				++p;

			if (*p != '=')
				continue;
			p++;

			p[strcspn(p,"\r\n")] = 0;

			if (p[0] == '"' || p[0] == '\'') {
				p++;
				p[strlen(p) - 1]  = 0;
			}

			strncpy(value, p, size);
			ret = strlen(value);
			break;
		}
	}

	fclose(cfg_file);
	return ret;
}


static int handleConfiguration(const struct ifaddrs *ifa)
{
	int i;
	eth_data_t *en;

	for (i = 0; i < IF_NUM; ++i) {
		if (strncmp(ifa->ifa_name, if_strings[i], strlen(if_strings[i])) == 0)
			break;
	}

	if (i >= IF_NUM || eth_common.if_mask & FLAG(i))
		return -1;

	eth_common.if_mask |= FLAG(i);
	en = &eth_common.en[i];
	en->data_mask = 0;

	strncpy(en->ifname, ifa->ifa_name, sizeof(en->ifname));
	en->data_mask |= FLAG(eth_ifname);

	en->flags = ifa->ifa_flags;
	en->data_mask |= FLAG(eth_flags);

	if (ifa->ifa_addr != NULL) {
		inet_ntop(AF_INET, &(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr), en->ip, sizeof(en->ip));
		en->data_mask |= FLAG(eth_ip);
	}

	if (ifa->ifa_netmask != NULL) {
		inet_ntop(AF_INET, &(((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr), en->mask, sizeof(en->mask));
		en->data_mask |= FLAG(eth_mask);
	}

	if (ifa->ifa_broadaddr != NULL) {
		inet_ntop(AF_INET, &(((struct sockaddr_in *)ifa->ifa_broadaddr)->sin_addr), en->brdstaddr, sizeof(en->brdstaddr));
		en->data_mask |= FLAG(eth_brdst);
	}

	return 0;
}


static int eth_update(void)
{
	time_t curr;
	struct ifaddrs *ifaddr, *ifap;

	gettime(&curr, NULL);

	if (curr < eth_common.last_update + UPDATE_CACHE)
		return 0;

	eth_common.if_mask = 0;

	if (getifaddrs(&ifaddr) < 0) {
		return -1;
	}

	ifap = ifaddr;
	do {
		if (ifap->ifa_addr == NULL || ifap->ifa_addr->sa_family != AF_INET) {
			continue;
		}
		handleConfiguration(ifap);

	}
	while ((ifap = ifap->ifa_next) != NULL);

	freeifaddrs(ifaddr);
	eth_common.last_update = curr;

	return 0;
}


static inline int check_updated(int iface, int value)
{
	return eth_common.if_mask & FLAG(iface) && eth_common.en[iface].data_mask & FLAG(value);
}


void oledfun_get_en1_ip(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en1, eth_ip)) {
		strcpy(dst, "IP: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "IP: %s", eth_common.en[if_en1].ip);
	return;
}


void oledfun_get_en1_mask(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en1, eth_mask)) {
		strcpy(dst, "Msk: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Msk: %s", eth_common.en[if_en1].mask);
	return;
}


void oledfun_get_en1_status(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en1, eth_flags)) {
		strcpy(dst, "Status: ERROR");
		return;
	}

	status = eth_common.en[if_en1].flags & IFF_UP;

	snprintf(dst, oledfun_common.max_len, "Status: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_en1_link(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en1, eth_flags)) {
		strcpy(dst, "Link: ERROR");
		return;
	}

	status = eth_common.en[if_en1].flags & IFF_RUNNING;

	snprintf(dst, oledfun_common.max_len, "Link: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_en1_type(char *dst, int exec)
{
	int type;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en1, eth_flags)) {
		strcpy(dst, "Link: ERROR");
		return;
	}

	type = eth_common.en[if_en1].flags & IFF_DYNAMIC;

	snprintf(dst, oledfun_common.max_len, "Conn. type: %s", type ? "DHCP" : "Static");
	return;
}


void oledfun_get_en2_ip(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en2, eth_ip)) {
		strcpy(dst, "IP: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "IP: %s", eth_common.en[if_en2].ip);
	return;
}


void oledfun_get_en2_mask(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en2, eth_mask)) {
		strcpy(dst, "Msk: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Msk: %s", eth_common.en[if_en2].mask);
	return;
}


void oledfun_get_en2_status(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en2, eth_flags)) {
		strcpy(dst, "Status: ERROR");
		return;
	}

	status = eth_common.en[if_en2].flags & IFF_UP;

	snprintf(dst, oledfun_common.max_len, "Status: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_en2_link(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en2, eth_flags)) {
		strcpy(dst, "Link: ERROR");
		return;
	}

	status = eth_common.en[if_en2].flags & IFF_RUNNING;

	snprintf(dst, oledfun_common.max_len, "Link: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_en2_type(char *dst, int exec)
{
	int type;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_en2, eth_flags)) {
		strcpy(dst, "Link: ERROR");
		return;
	}

	type = eth_common.en[if_en2].flags & IFF_DYNAMIC;

	snprintf(dst, oledfun_common.max_len, "Conn. type: %s", type ? "DHCP" : "Static");
	return;
}


void oledfun_get_gsm_ifname(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_gsm, eth_ifname)) {
		strcpy(dst, "GSM: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "GSM: %s", eth_common.en[if_gsm].ifname);
	return;
}


void oledfun_get_gsm_status(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_gsm, eth_flags)) {
		strcpy(dst, "Status: ERROR");
		return;
	}

	status = eth_common.en[if_gsm].flags & IFF_UP;

	snprintf(dst, oledfun_common.max_len, "Status: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_gsm_media(char *dst, int exec)
{
	char keyname[10];
	char media[13];

	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_gsm, eth_ifname)) {
		strcpy(dst, "Media: ERROR");
		return;
	}

	snprintf(keyname, sizeof(keyname), "%s_media", eth_common.en[if_gsm].ifname);

	if (find_in_config_file(IFSTATUS_PATH, keyname, media, sizeof(media)) < 0) {
		strcpy(dst, "Media: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "Media: %s", media);
	return;
}


void oledfun_get_gsm_apn(char *dst, int exec)
{
	char apn[15];

	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_gsm, eth_ifname)) {
		strcpy(dst, "APN: ERROR");
		return;
	}

	if (find_in_config_file(GSM_CFG_PATH, "apn", apn, sizeof(apn)) < 0) {
		strcpy(dst, "APN: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "APN: %s", apn);
	return;
}


void oledfun_get_gsm_ip(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_gsm, eth_ip)) {
		strcpy(dst, "IP: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "IP: %s", eth_common.en[if_gsm].ip);
	return;
}


void oledfun_get_gsm_link(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_gsm, eth_flags)) {
		strcpy(dst, "Link: ERROR");
		return;
	}

	status = eth_common.en[if_gsm].flags & IFF_RUNNING;

	snprintf(dst, oledfun_common.max_len, "Link: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_vpn_status(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_vpn, eth_flags)) {
		strcpy(dst, "Status: ERROR");
		return;
	}

	status = eth_common.en[if_vpn].flags & IFF_UP;

	snprintf(dst, oledfun_common.max_len, "Status: %s", status ? "UP" : "DOWN");
	return;
}


void oledfun_get_vpn_ifname(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_vpn, eth_ifname)) {
		strcpy(dst, "VPN: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "VPN: %s", eth_common.en[if_vpn].ifname);
	return;
}


void oledfun_get_vpn_ip(char *dst, int exec)
{
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_vpn, eth_ip)) {
		strcpy(dst, "IP: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "IP: %s", eth_common.en[if_vpn].ip);
	return;
}


void oledfun_get_vpn_link(char *dst, int exec)
{
	int status;
	if (exec)
		return;

	if (eth_update() < 0 || !check_updated(if_vpn, eth_flags)) {
		strcpy(dst, "Link: ERROR");
		return;
	}

	status = eth_common.en[if_vpn].flags & IFF_RUNNING;

	snprintf(dst, oledfun_common.max_len, "Link: %s", status ? "UP" : "DOWN");
	return;
}
