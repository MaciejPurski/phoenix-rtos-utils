/*
 * Phoenix-RTOS
 *
 * i.MX 6ULL AD7779 driver.
 *
 * Copyright 2018 Phoenix Systems
 * Author: Krystian Wasik
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "ecspi.h"
#include "ad7779.h"
#include "adc-api.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sdma.h>

#include <sys/stat.h>
#include <sys/threads.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/platform.h>

#include <phoenix/arch/imx6ull.h>

#define COL_RED     "\033[1;31m"
#define COL_CYAN    "\033[1;36m"
#define COL_NORMAL  "\033[0m"

#define LOG_TAG "ad7779-drv: "
#define log_debug(fmt, ...)     do { printf(LOG_TAG fmt "\n", ##__VA_ARGS__); } while (0)
#define log_info(fmt, ...)      do { printf(LOG_TAG COL_CYAN fmt COL_NORMAL "\n", ##__VA_ARGS__); } while (0)
#define log_error(fmt, ...)     do { printf(LOG_TAG COL_RED fmt COL_NORMAL "\n", ##__VA_ARGS__); } while (0)

#define SDMA_DEVICE_FILE_NAME           ("/dev/sdma/ch07")

#define ADC_BUFFER_SIZE                 (SIZE_PAGE)

typedef volatile struct {
	uint32_t TCSR;
	uint32_t TCR1;
	uint32_t TCR2;
	uint32_t TCR3;
	uint32_t TCR4;
	uint32_t TCR5;
	uint32_t reserved0[2];
	uint32_t TDR0;
	uint32_t reserved1[7];
	uint32_t TFR0;
	uint32_t reserved2[7];
	uint32_t TMR;
	uint32_t reserved3[7];
	uint32_t RCSR;
	uint32_t RCR1;
	uint32_t RCR2;
	uint32_t RCR3;
	uint32_t RCR4;
	uint32_t RCR5;
	uint32_t reserved4[2];
	uint32_t RDR0;
	uint32_t reserved5[7];
	uint32_t RFR0;
	uint32_t reserved6[7];
	uint32_t RMR;
	uint32_t reserved7[7];
	uint32_t MCR;
} sai_t;

struct driver_common_s
{
	uint32_t port;

	addr_t buffer0_paddr;
	addr_t buffer1_paddr;

	sdma_t sdma;

	sdma_buffer_desc_t *bd;

	sai_t *sai;
	addr_t sai_paddr;

	int enabled;
} common;

#define SAI_FIFO_WATERMARK              (4)

#define SAI_RCR3_RCE_BIT                (1 << 16)
#define SAI_RCSR_RE_BIT                 (1 << 31)
#define SAI_RCSR_FRDE_BIT               (1 << 0)

static int sai_init(void)
{
	common.sai_paddr = 0x2030000; /* SAI3 */
	common.sai = mmap(NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, MAP_DEVICE, OID_PHYSMEM, common.sai_paddr);
	if (common.sai == MAP_FAILED)
		return -1;

	platformctl_t pctl;
	pctl.action = pctl_set;
	pctl.type = pctl_devclock;
	pctl.devclock.dev = pctl_clk_sai3;
	pctl.devclock.state = 0b11;
	platformctl(&pctl);

	pctl.action = pctl_set;
	pctl.type = pctl_iomux;
	pctl.iomux.mux = pctl_mux_lcd_d10;
	pctl.iomux.sion = 0;
	pctl.iomux.mode = 1; /* ALT1 (SAI3_RX_SYNC) */
	platformctl(&pctl);

	pctl.action = pctl_set;
	pctl.type = pctl_iomux;
	pctl.iomux.mux = pctl_mux_lcd_d11;
	pctl.iomux.sion = 0;
	pctl.iomux.mode = 1; /* ALT1 (SAI3_RX_BCLK) */
	platformctl(&pctl);

	pctl.action = pctl_set;
	pctl.type = pctl_iomux;
	pctl.iomux.mux = pctl_mux_lcd_d14;
	pctl.iomux.sion = 0;
	pctl.iomux.mode = 1; /* ALT1 (SAI3_RX_DATA) */
	platformctl(&pctl);

	pctl.action = pctl_set;
	pctl.type = pctl_ioisel;
	pctl.ioisel.isel = pctl_isel_sai3_rx;
	pctl.ioisel.daisy = 1; /* Select LCD_DATA14 pad */
	platformctl(&pctl);

	/* Initialize SAI */
	// sai->MCR |= 1 << 30; /* Set MCLK Output Enable */
	common.sai->RCR1 = SAI_FIFO_WATERMARK;
	common.sai->RCR2 = 0x0; /* External bit clock (slave mode) */
	common.sai->RCR3 |= SAI_RCR3_RCE_BIT;
	common.sai->RCR4 = 0x00070018; /* FRSZ=7, SYWD=0, MF=1, FSE=1, FSP=0, FSD=0 */
	common.sai->RCR5 = 0x1f1f1f00; /* WNW=31, WOW=31, FBT=31 */
	common.sai->RMR = 0x0; /* No words masked */
	common.sai->RCSR |= SAI_RCSR_FRDE_BIT; /* FIFO Request DMA Enable */

	return 0;
}

static void sai_rx_enable(void)
{
	common.sai->RCSR |= SAI_RCSR_RE_BIT;
}

static addr_t sai_get_rx_fifo_ptr(void)
{
	return (addr_t)(&(((sai_t*)common.sai_paddr)->RDR0));
}

static int sdma_configure(void)
{
	int res;

	/*
	 * WARNING!
	 *
	 * SDMA channel responsible for reading ADC samples from SAI RX FIFO
	 * is actually triggered by EPIT2 event (which is used by PLCIO). Only then
	 * SDMA script checks if SAI RX FIFO event flag is set.
	 *
	 * The reason for this is that SDMA is not truly preemptive. For higher
	 * priority channel to be switched in while other SDMA script is running,
	 * said script has to execute yield/done instruction. Triggering this
	 * channel directly by the SAI RX FIFO event results in PLCIO channel
	 * sometimes being delayed (when SAI RX FIFO event occurrs just before EPIT2
	 * event).
	 */
	uint8_t event_transfer = 39; /* SAI3 RX FIFO */;
	uint8_t event_channel = 2; /* EPIT2 */

	unsigned tries = 25;
	while ((res = sdma_open(&common.sdma, SDMA_DEVICE_FILE_NAME)) < 0) {
		usleep(100*1000);
		if (--tries == 0) {
			log_error("failed to open SDMA device file (%s)", SDMA_DEVICE_FILE_NAME);
			return -1;
		}
	}

	void *buffer0, *buffer1;
	buffer0 = sdma_alloc_uncached(&common.sdma, ADC_BUFFER_SIZE, &common.buffer0_paddr, 1);
	buffer1 = sdma_alloc_uncached(&common.sdma, ADC_BUFFER_SIZE, &common.buffer1_paddr, 1);
	if (buffer0 == NULL || buffer1 == NULL) {
		log_error("failed to allocate buffers");
		return -1;
	}

	addr_t bd_paddr;
	common.bd = sdma_alloc_uncached(&common.sdma, 2*sizeof(sdma_buffer_desc_t), &bd_paddr, 1);
	if (common.bd == NULL) {
		log_error("failed to allocate memory for buffer descriptors");
		return -1;
	}

	common.bd[0].count = ADC_BUFFER_SIZE;
	common.bd[0].flags = SDMA_BD_DONE | SDMA_BD_INTR;
	common.bd[0].command = SDMA_CMD_MODE_32_BIT;
	common.bd[0].buffer_addr = common.buffer0_paddr;

	common.bd[1].count = ADC_BUFFER_SIZE;
	common.bd[1].flags = SDMA_BD_DONE | SDMA_BD_WRAP | SDMA_BD_INTR;
	common.bd[1].command = SDMA_CMD_MODE_32_BIT;
	common.bd[1].buffer_addr = common.buffer1_paddr;

	/* SDMA context setup */
	sdma_context_t sdma_context;
	sdma_context_init(&sdma_context);
	sdma_context_set_pc(&sdma_context, sdma_script__shp_2_mcu);
	if (event_transfer < 32) {
		sdma_context.gr[1] = 1 << event_transfer;
	} else {
		sdma_context.gr[0] = 1 << (event_transfer - 32); /* Event2_mask */
	}
	sdma_context.gr[6] = sai_get_rx_fifo_ptr(); /* RX FIFO address */
	sdma_context.gr[7] = SAI_FIFO_WATERMARK * sizeof(uint32_t); /* Watermark level */

	/* Load channel context */
	sdma_context_set(&common.sdma, &sdma_context);

	sdma_channel_config_t cfg;
	cfg.bd_paddr = bd_paddr;
	cfg.bd_cnt = 2;
	cfg.trig = sdma_trig__event;
	cfg.event = event_channel;
	cfg.priority = SDMA_CHANNEL_PRIORITY_MIN + 1;
	sdma_channel_configure(&common.sdma, &cfg);

	sdma_enable(&common.sdma);

	return 0;
}

static int dev_init(void)
{
	int res;
	oid_t dir;
	msg_t msg;

	res = portCreate(&common.port);
	if (res != EOK) {
		log_error("could not create port: %d", res);
		return -1;
	}

	res = mkdir(ADC_DEVICE_DIR, 0);
	if (res < 0 && errno != EEXIST) {
		log_error("mkdir /dev failed (%d)", -errno);
		return -1;
	}

	if ((res = lookup(ADC_DEVICE_DIR, NULL, &dir)) < 0) {
		log_error("%s lookup failed (%d)", ADC_DEVICE_DIR, res);
		return -1;
	}

	msg.type = mtCreate;
	msg.i.create.type = otDev;
	msg.i.create.mode = 0;
	msg.i.create.dev.port = common.port;
	msg.i.create.dev.id = 0;
	msg.i.create.dir = dir;
	msg.i.data = ADC_DEVICE_FILE_NAME;
	msg.i.size = sizeof(ADC_DEVICE_FILE_NAME);
	msg.o.data = NULL;
	msg.o.size = 0;

	if ((res = msgSend(dir.port, &msg)) < 0 || msg.o.create.err != EOK) {
		log_error("could not create %s (res=%d, err=%d)", ADC_DEVICE_FILE_NAME, res, msg.o.create.err);
		return -1;
	}

	log_info("device initialized");

	return 0;
}

static int dev_open(oid_t *oid, int flags)
{
	(void)oid;
	(void)flags;

	return EOK;
}

static int dev_close(oid_t *oid, int flags)
{
	(void)oid;
	(void)flags;

	return EOK;
}

static int dev_read(void *data, size_t size)
{
	if (data != NULL && size != sizeof(unsigned))
		return -EIO;

	if (sdma_wait_for_intr(&common.sdma, data) < 0)
		return -EIO;

	return EOK;
}

static int dev_ctl(msg_t *msg)
{
	int res;
	adc_dev_ctl_t dev_ctl;

	memcpy(&dev_ctl, msg->o.raw, sizeof(adc_dev_ctl_t));

	switch (dev_ctl.type) {
		case adc_dev_ctl__enable:
			common.enabled = 1;
			sai_rx_enable();
			return EOK;

		case adc_dev_ctl__set_config:
			if (common.enabled)
				return -EBUSY;
			res = ad7779_set_sampling_rate(dev_ctl.config.sampling_rate);
			if (res == AD7779_ARG_ERROR)
				return -EINVAL;
			if (res != AD7779_OK)
				return -EIO;
			return EOK;

		case adc_dev_ctl__get_config:
			res = ad7779_get_sampling_rate(&dev_ctl.config.sampling_rate);
			if (res != AD7779_OK)
				return -EIO;
			dev_ctl.config.channels = AD7779_NUM_OF_CHANNELS;
			dev_ctl.config.bits = AD7779_NUM_OF_BITS;
			memcpy(msg->o.raw, &dev_ctl, sizeof(adc_dev_ctl_t));
			return EOK;

		case adc_dev_ctl__get_buffers:
			dev_ctl.buffers.paddr0 = common.buffer0_paddr;
			dev_ctl.buffers.paddr1 = common.buffer1_paddr;
			dev_ctl.buffers.size = ADC_BUFFER_SIZE;
			memcpy(msg->o.raw, &dev_ctl, sizeof(adc_dev_ctl_t));
			return EOK;

		case adc_dev_ctl__set_channel_gain:
			res = ad7779_set_channel_gain(dev_ctl.gain.channel, dev_ctl.gain.val);
			if (res == AD7779_ARG_ERROR)
				return -EINVAL;
			if (res != AD7779_OK)
				return -EIO;
			return EOK;

		case adc_dev_ctl__get_channel_gain:
			res = ad7779_get_channel_gain(dev_ctl.gain.channel, &dev_ctl.gain.val);
			if (res == AD7779_ARG_ERROR)
				return -EINVAL;
			if (res != AD7779_OK)
				return -EIO;
			memcpy(msg->o.raw, &dev_ctl, sizeof(adc_dev_ctl_t));
			return EOK;

		default:
			log_error("dev_ctl: unknown type (%d)", dev_ctl.type);
			return -ENOSYS;
	}

	return EOK;
}

static void msg_loop(void)
{
	msg_t msg;
	unsigned rid;

	while (1) {
		if (msgRecv(common.port, &msg, &rid) < 0)
			continue;

		switch (msg.type) {
			case mtOpen:
				msg.o.io.err = dev_open(&msg.i.openclose.oid, msg.i.openclose.flags);
				break;

			case mtClose:
				msg.o.io.err = dev_close(&msg.i.openclose.oid, msg.i.openclose.flags);
				break;

			case mtRead:
				msg.o.io.err = dev_read(msg.o.data, msg.o.size);
				break;

			case mtWrite:
				msg.o.io.err = -ENOSYS;
				break;

			case mtDevCtl:
				msg.o.io.err = dev_ctl(&msg);
				break;
		}

		msgRespond(common.port, &msg, rid);
	}
}

static int init(void)
{
	int res;

	common.enabled = 0;

	if ((res = ecspi_init()) < 0) {
		log_error("failed to initialize ecspi");
		return res;
	}

	if ((res = sai_init()) < 0) {
		log_error("failed to initialize sai");
		return res;
	}

	if ((res = ad7779_init()) < 0) {
		log_error("failed to initialize ad7779 (%d)", res);
		return res;
	}

	if ((res = sdma_configure()) < 0) {
		log_error("failed to configure sdma");
		return res;
	}

	if ((res = dev_init()) < 0) {
		log_error("device initialization failed");
		return res;
	}

	return 0;
}

int main(void)
{
	oid_t root;

	/* Wait for the filesystem */
	while (lookup("/", NULL, &root) < 0)
		usleep(10000);

	if (init())
		return -EIO;

	msg_loop();

	/* Should never be reached */
	log_error("Exiting!");
	return 0;
}
