#include <sys/msg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "afe03x.h"
#include "../../phoenix-rtos-devices/multi/imxrt-multi/imxrt-multi.h"


#define LOG(str, ...) do { if (1) fprintf(stderr, "afe: " str "\n", ##__VA_ARGS__); } while (0)
#define LOG_ERROR(str, ...) do { fprintf(stderr, __FILE__  ":%d error: " str "\n", __LINE__, ##__VA_ARGS__); } while (0)


struct {
	oid_t dir;
} afe_common;


static oid_t afe_getOid(void)
{
	oid_t dir;
	while (lookup("/dev/spi3", NULL, &dir) < 0)
		usleep(9000);

	return dir;
}


static int afe_spiConfig(void)
{
	msg_t msg;
	multi_i_t *idevctl = NULL;
	multi_o_t *odevctl = NULL;

	msg.type = mtDevCtl;
	msg.i.data = NULL;
	msg.i.size = 0;
	msg.o.data = NULL;
	msg.o.size = 0;

	idevctl = (multi_i_t *)msg.i.raw;
	idevctl->id = afe_common.dir.id;
	idevctl->spi.type = spi_config;
	idevctl->spi.config.cs = 0;
	idevctl->spi.config.endian = spi_msb;
	idevctl->spi.config.mode = spi_mode_0;
	idevctl->spi.config.prescaler = 2;
	idevctl->spi.config.sckDiv = 8;

	odevctl = (multi_o_t *)msg.o.raw;

	if (msgSend(afe_common.dir.port, &msg) < 0)
		return -1;

	if (odevctl->err < 0)
		return -1;

	return EOK;
}



static int afe_spiTransmit(const uint8_t *tx, uint8_t *rx, int sz)
{
	msg_t msg;
	multi_i_t *idevctl = NULL;
	multi_o_t *odevctl = NULL;

	msg.type = mtDevCtl;
	msg.i.data = tx;
	msg.i.size = sz;
	msg.o.data = rx;
	msg.o.size = 0;

	idevctl = (multi_i_t *)msg.i.raw;
	idevctl->id = afe_common.dir.id;
	idevctl->spi.type = spi_transaction;
	idevctl->spi.transaction.frameSize = sz;
	idevctl->spi.transaction.cs = 0;

	odevctl = (multi_o_t *)msg.o.raw;

	if (msgSend(afe_common.dir.port, &msg) < 0)
		return -1;

	if (odevctl->err < 0)
		return -1;

	return odevctl->err;
}


static int exchange(const uint8_t *out, uint8_t *in, uint16_t len)
{
	return afe_spiTransmit(out, in, len);
}


int main(int argc, char **argv)
{
	ps_afe03x_cfg_t cfg;

	afe_common.dir = afe_getOid();
	cfg.spi_exchange = exchange;

	afe_spiConfig();

	if (ps_afe03x_init(&cfg) < 0)
		LOG_ERROR("ERROR - doesn't work!!!");
	else
		LOG("AFE works correctly.");

    return 0;
}
