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

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <libsystem_incotex.h>

#include "helper.h"

#include "plc.h"

#define UPDATE_CACHE (2 * 1000 * 1000)

static struct {
	inc_prime_stats_t stats;
	time_t last_update;
} plc_comnon;


static int update_plc(void)
{
	time_t curr;

	gettime(&curr, NULL);

	if (curr < plc_comnon.last_update + UPDATE_CACHE)
		return 0;

	if (inc_getPrimeStats(&plc_comnon.stats) < 0) {
		return -1;
	}

	plc_comnon.last_update = curr;

	return 0;
}


/* TODO stub */
void oledfun_get_plc_snr(char *dst, int exec)
{
	if (exec)
		return;

	strcpy(dst, "SNR: ERROR");
	return;
}


void oledfun_get_plc_txbs(char *dst, int exec)
{
	if (exec)
		return;

	if (update_plc() < 0) {
		strcpy(dst, "TXBS: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "TXBS: %u", plc_comnon.stats.tx_bcn_success_cnt);
}


void oledfun_get_plc_txds(char *dst, int exec)
{
	if (exec)
		return;

	if (update_plc() < 0) {
		strcpy(dst, "TXDS: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "TXDS: %u", plc_comnon.stats.tx_scp_success_cnt);
}


void oledfun_get_plc_txbf(char *dst, int exec)
{
	if (exec)
		return;

	if (update_plc() < 0) {
		strcpy(dst, "TXBF: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "TXBF: %u", plc_comnon.stats.tx_bcn_fail_cnt);
}


void oledfun_get_plc_txdf(char *dst, int exec)
{
	if (exec)
		return;

	if (update_plc() < 0) {
		strcpy(dst, "TXDF: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "TXDF: %u", plc_comnon.stats.tx_scp_fail_cnt);
}


void oledfun_get_plc_rx(char *dst, int exec)
{
	if (exec)
		return;

	if (update_plc() < 0) {
		strcpy(dst, "RX: ERROR");
		return;
	}

	snprintf(dst, oledfun_common.max_len, "RX: %u", plc_comnon.stats.rx_cnt);
}
