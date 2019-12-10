/*
 * Phoenix-RTOS
 *
 * i.MX RT AD7779 driver.
 *
 * Copyright 2018, 2019 Phoenix Systems
 * Author: Krystian Wasik, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "ad7779.h"
#include "../../phoenix-rtos-devices/multi/imxrt-multi/imxrt-multi.h"

#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/platform.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

#include <phoenix/arch/imxrt.h>

#define COL_RED     "\033[1;31m"
#define COL_CYAN    "\033[1;36m"
#define COL_NORMAL  "\033[0m"

#define LOG_TAG "ad7779-drv: "

#define log_info(fmt, ...)      do { printf(COL_CYAN LOG_TAG fmt COL_NORMAL "\n", ##__VA_ARGS__); } while (0)
#define log_error(fmt, ...)     do { printf(COL_RED  LOG_TAG fmt COL_NORMAL "\n", ##__VA_ARGS__); } while (0)

#if 1
#define log_debug(fmt, ...)     do { printf(LOG_TAG fmt "\n", ##__VA_ARGS__); } while (0)
#else
#define log_debug(fmt, ...)
#endif

#define AD7779_CHn_CONFIG(n)                (0x00 + n)
#define CHn_GAIN_SHIFT                      (6)

#define AD7779_CH_DISABLE                   (0x08)

#define AD7779_GENERAL_USER_CONFIG_1        (0x11)
#define POWERMODE_BIT                       (1 << 6)

#define AD7779_GENERAL_USER_CONFIG_2        (0x12)
#define SPI_SYNC                            (1 << 0)

#define AD7779_GENERAL_USER_CONFIG_3        (0x13)

#define AD7779_DOUT_FORMAT                  (0x14)

#define AD7779_ADC_MUX_CONFIG               (0x15)

#define AD7779_CH0_ERR_REG                  (0x4C)
#define AD7779_CH1_ERR_REG                  (0x4D)
#define AD7779_CH2_ERR_REG                  (0x4E)
#define AD7779_CH3_ERR_REG                  (0x4F)
#define AD7779_CH4_ERR_REG                  (0x50)
#define AD7779_CH5_ERR_REG                  (0x51)
#define AD7779_CH6_ERR_REG                  (0x52)
#define AD7779_CH7_ERR_REG                  (0x53)
#define AD7779_CH0_1_SAT_ERR                (0x54)
#define AD7779_CH2_3_SAT_ERR                (0x55)
#define AD7779_CH4_5_SAT_ERR                (0x56)
#define AD7779_CH6_7_SAT_ERR                (0x57)
#define AD7779_GEN_ERR_REG_1                (0x59)
#define AD7779_GEN_ERR_REG_2                (0x5B)
#define AD7779_STATUS_REG_1                 (0x5D)
#define AD7779_STATUS_REG_2                 (0x5E)
#define AD7779_STATUS_REG_3                 (0x5F)

#define AD7779_SRC_N_MSB                    (0x60)
#define AD7779_SRC_N_LSB                    (0x61)
#define AD7779_SRC_IF_MSB                   (0x62)
#define AD7779_SRC_IF_LSB                   (0x63)

#define AD7779_SRC_UPDATE                   (0x64)
#define SRC_LOAD_SOURCE_BIT                 (1 << 7)
#define SRC_LOAD_UPDATE_BIT                 (1 << 0)

#define AD7779_MCLK_FREQ                    ((uint32_t)8192*1000)

#define AD7779_MAX_SAMPLE_RATE_LP           (8000)
#define AD7779_MAX_SAMPLE_RATE_HR           (16000)


static struct {
	oid_t multidrv;
} ad7779_common;


/* Pin config:
 * /START -> GPIO_B0_04 (ALT5 GPIO2_IO04)
 * /RESET -> GPIO_B0_05 (ALT5 GPIO2_IO05)
 * /DRDY -> GPIO_B0_14 (ALT3 SAI1_RX_SYNC)
 * DCLK -> GPIO_B0_15 (ALT3 SAI_RX_BCLK)
 * DOUT3 -> GPIO_B0_12 (ALT3 SAI1_TX_DATA01)
 * DOUT2 -> GPIO_B0_11 (ALT3 SAI1_TX_DATA02)
 * DOUT1 -> GPIO_B0_10 (ALT3 SAI1_TX_DATA03)
 * DOUT0 -> GPIO_B1_00 (ALT3 SAI1_TX_DATA00)
 * /CS -> GPIO_B0_00 (managed by Multidrv)
 * SCLK -> GPIO_B0_03 (managed by Multidrv)
 * SDO -> GPIO_B0_01 (managed by Multidrv)
 * SDI -> GPIO_B0_02 (managed by Multidrv)
 * CLK_SEL -> GPIO_B0_06 (ALT5 GPIO2_IO06)
 */


static void gpio_setPin(int gpio, int pin, int state)
{
	msg_t msg;
	multi_i_t imsg;

	msg.type = mtWrite;
	msg.i.data = &imsg;
	msg.i.size = sizeof(imsg);
	msg.o.data = NULL;
	msg.o.size = 0;

	imsg.type = gpio;
	imsg.gpio.type = gpio_port;
	imsg.gpio.port.val = !!state << pin;
	imsg.gpio.port.mask = 1 << pin;

	msgSend(ad7779_common.multidrv.port, &msg);
}

#if 0
static int gpio_getPin(int gpio, int pin)
{
	msg_t msg;
	multi_i_t imsg;
	multi_o_t omsg;

	msg.type = mtRead;
	msg.i.data = &imsg;
	msg.i.size = sizeof(imsg);
	msg.o.data = &omsg;
	msg.o.size = sizeof(omsg);

	imsg.type = gpio;
	imsg.gpio.type = gpio_port;

	msgSend(ad7779_common.multidrv.port, &msg);

	return !!(omsg.val & (1 << pin));
}
#endif

static void gpio_setDir(int gpio, int pin, int dir)
{
	msg_t msg;
	multi_i_t imsg;

	msg.type = mtWrite;
	msg.i.data = &imsg;
	msg.i.size = sizeof(imsg);
	msg.o.data = NULL;
	msg.o.size = 0;

	imsg.type = gpio;
	imsg.gpio.type = gpio_dir;
	imsg.gpio.port.val = !!dir << pin;
	imsg.gpio.port.mask = 1 << pin;

	msgSend(ad7779_common.multidrv.port, &msg);
}


static int lpspi_transaction(char *buff, size_t bufflen)
{
	msg_t msg;
	multi_i_t *imsg = (multi_i_t *)msg.i.raw;
	multi_o_t *omsg = (multi_o_t *)msg.o.raw;

	msg.type = mtDevCtl;
	msg.i.data = buff;
	msg.i.size = bufflen;
	msg.o.data = buff;
	msg.o.size = bufflen;

	imsg->type = id_spi4;
	imsg->spi.type = spi_transaction;
	imsg->spi.transaction.cs = 0;
	imsg->spi.transaction.frameSize = bufflen;

	msgSend(ad7779_common.multidrv.port, &msg);

	return omsg->err;
}


static int lpspi_config(void)
{
	msg_t msg;
	multi_i_t *imsg = (multi_i_t *)msg.i.raw;

	msg.type = mtDevCtl;
	msg.i.data = NULL;
	msg.i.size = 0;
	msg.o.data = NULL;
	msg.o.size = 0;

	imsg->type = id_spi4;
	imsg->spi.type = spi_config;
	imsg->spi.config.cs = 0;
	imsg->spi.config.mode = spi_mode_0;
	imsg->spi.config.endian = spi_msb;
	imsg->spi.config.sckDiv = 0;
	imsg->spi.config.prescaler = 7;

	return msgSend(ad7779_common.multidrv.port, &msg);
}


#define AD7779_READ_BIT         (0x80)

static int ad7779_read(uint8_t addr, uint8_t *data, uint8_t len)
{
	uint8_t buff[len + 1];

	if (len == 0)
		return AD7779_OK;

	buff[0] = addr | AD7779_READ_BIT;
	memset(buff + 1, 0, len);
	if (lpspi_transaction((void *)buff, len + 1) < 0)
		return AD7779_CTRL_IO_ERROR;

	memcpy(data, buff + 1, len);

	if (buff[0] != 0x20)
		return AD7779_CTRL_HEADER_ERROR;

	return AD7779_OK;
}


static int __attribute__((unused)) ad7779_read_reg(uint8_t addr, uint8_t *val)
{
	return ad7779_read(addr, val, sizeof(uint8_t));
}


static int ad7779_write(uint8_t addr, const uint8_t *data, uint8_t len)
{
	uint8_t buff[len + 1];

	if (len == 0)
		return AD7779_OK;

	buff[0] = addr;
	memcpy(buff + 1, data, len);

	if (lpspi_transaction((void *)buff, len + 1) < 0)
		return AD7779_CTRL_IO_ERROR;

	if (buff[0] != 0x20)
		return AD7779_CTRL_HEADER_ERROR;

	return AD7779_OK;
}


static int ad7779_write_reg(uint8_t addr, uint8_t val)
{
	return ad7779_write(addr, &val, sizeof(val));
}


static int ad7779_set_clear_bits(uint8_t addr, uint8_t set, uint8_t clear)
{
	int res;
	uint8_t reg_val, reg_val_ver;

	res = ad7779_read(addr, &reg_val, sizeof(reg_val));
	if (res < 0)
		return res;

	reg_val |= set;
	reg_val &= ~clear;

	res = ad7779_write(addr, &reg_val, sizeof(reg_val));
	if (res < 0)
		return res;

	res = ad7779_read(addr, &reg_val_ver, sizeof(reg_val_ver));
	if (res < 0)
		return res;

	if (reg_val != reg_val_ver)
		return AD7779_VERIFY_FAILED;

	return 0;
}


int ad7779_get_mode(ad7779_mode_t *mode)
{
	int res;
	uint8_t reg;

	if (mode == NULL)
		return AD7779_ARG_ERROR;

	res = ad7779_read_reg(AD7779_GENERAL_USER_CONFIG_1, &reg);
	if (res != AD7779_OK)
		return res;

	if (reg & POWERMODE_BIT) {
		*mode = ad7779_mode__high_resolution;
	} else {
		*mode = ad7779_mode__low_power;
	}

	return AD7779_OK;
}

int ad7779_set_mode(ad7779_mode_t mode)
{
	if (mode == ad7779_mode__high_resolution)
		return ad7779_set_clear_bits(AD7779_GENERAL_USER_CONFIG_1, POWERMODE_BIT, 0);
	else
		return ad7779_set_clear_bits(AD7779_GENERAL_USER_CONFIG_1, 0, POWERMODE_BIT);
}

int ad7779_get_sampling_rate(uint32_t *fs)
{
	int res;

	if (fs == NULL)
		return AD7779_ARG_ERROR;

	ad7779_mode_t mode;
	if ((res = ad7779_get_mode(&mode)) < 0)
		return res;

	uint32_t base = AD7779_MCLK_FREQ/4;
	if (mode == ad7779_mode__low_power) base = base/2;

	uint8_t SRC_N_MSB, SRC_N_LSB, SRC_IF_MSB, SRC_IF_LSB;
	if ((res = ad7779_read_reg(AD7779_SRC_N_MSB, &SRC_N_MSB)) < 0)
		return res;
	if ((res = ad7779_read_reg(AD7779_SRC_N_LSB, &SRC_N_LSB)) < 0)
		return res;
	if ((res = ad7779_read_reg(AD7779_SRC_IF_MSB, &SRC_IF_MSB)) < 0)
		return res;
	if ((res = ad7779_read_reg(AD7779_SRC_IF_LSB, &SRC_IF_LSB)) < 0)
		return res;

	uint16_t SRC_N = ((uint16_t)SRC_N_MSB) << 8 | SRC_N_LSB; /* Decimation rate */
	uint16_t SRC_IF = ((uint16_t)SRC_IF_MSB) << 8 | SRC_IF_LSB; /* Interpolation factor */

	*fs = (((uint64_t)base) << 16)/(SRC_IF + (((uint64_t)SRC_N) << 16));

	log_debug("current sampling rate is %u (SRC_N=%u, SRC_IF=%u)", *fs, SRC_N, SRC_IF);

	return AD7779_OK;
}

int ad7779_set_sampling_rate(uint32_t fs)
{
	int res;

	ad7779_mode_t mode;
	if ((res = ad7779_get_mode(&mode)) < 0)
		return res;

	uint32_t base = AD7779_MCLK_FREQ/4;
	if (mode == ad7779_mode__low_power) base = base/2;

	/* Sanity check */
	if (mode == ad7779_mode__low_power && fs > AD7779_MAX_SAMPLE_RATE_LP) {
		log_debug("sampling rate too high (low power mode)");
		return AD7779_ARG_ERROR;
	}
	if (mode == ad7779_mode__high_resolution && fs > AD7779_MAX_SAMPLE_RATE_HR) {
		log_debug("sampling rate too high (high resolution mode)");
		return AD7779_ARG_ERROR;
	}

	uint16_t SRC_N = base/fs; /* Decimation rate */
	uint16_t SRC_IF = (base%fs << 16)/fs; /* Interpolation factor */

	log_debug("setting sampling rate to %u (SRC_N=%u, SRC_IF=%u)", fs, SRC_N, SRC_IF);

	// log_debug("clearing SRC_LOAD_UPDATE bit");
	if ((res = ad7779_set_clear_bits(AD7779_SRC_UPDATE, 0, SRC_LOAD_UPDATE_BIT)) < 0)
		return res;

	if ((res = ad7779_write_reg(AD7779_SRC_N_MSB,  (SRC_N  >> 8) & 0xff)) < 0)
		return res;
	if ((res = ad7779_write_reg(AD7779_SRC_N_LSB,   SRC_N        & 0xff)) < 0)
		return res;
	if ((res = ad7779_write_reg(AD7779_SRC_IF_MSB, (SRC_IF >> 8) & 0xff)) < 0)
		return res;
	if ((res = ad7779_write_reg(AD7779_SRC_IF_LSB,  SRC_IF       & 0xff)) < 0)
		return res;

	/* Trigger ODR update (by setting SRC_LOAD_UPDATE bit) */
	// log_debug("triggering ODR update by setting SRC_LOAD_UPDATE_BIT");
	if ((res = ad7779_set_clear_bits(AD7779_SRC_UPDATE, SRC_LOAD_UPDATE_BIT, 0)) < 0)
		return res;

	/* Reset internal logic */
	// log_debug("reseting internal logic");
	if ((res = ad7779_set_clear_bits(AD7779_GENERAL_USER_CONFIG_2, 0, SPI_SYNC)) < 0)
		return res;
	if ((res = ad7779_set_clear_bits(AD7779_GENERAL_USER_CONFIG_2, SPI_SYNC, 0)) < 0)
		return res;

	return AD7779_OK;
}

int ad7779_get_channel_gain(uint8_t channel, uint8_t *gain)
{
	int res;
	uint8_t reg;

	if (channel >= AD7779_NUM_OF_CHANNELS)
		return AD7779_ARG_ERROR;

	if ((res = ad7779_read_reg(AD7779_CHn_CONFIG(channel), &reg)) < 0)
		return res;

	if (gain != NULL)
		*gain = 1 << (reg >> CHn_GAIN_SHIFT);

	log_debug("current gain for channel %u is %u", channel, *gain);

	return AD7779_OK;
}

int ad7779_set_channel_gain(uint8_t channel, uint8_t gain)
{
	uint8_t reg;

	if (channel >= AD7779_NUM_OF_CHANNELS)
		return AD7779_ARG_ERROR;

	switch (gain) {
		case 1:
			reg = 0b00 << CHn_GAIN_SHIFT;
			break;
		case 2:
			reg = 0b01 << CHn_GAIN_SHIFT;
			break;
		case 4:
			reg = 0b10 << CHn_GAIN_SHIFT;
			break;
		case 8:
			reg = 0b11 << CHn_GAIN_SHIFT;
			break;
		default:
			return AD7779_ARG_ERROR;
	}

	log_debug("setting gain for channel %u to %u", channel, gain);

	return ad7779_write_reg(AD7779_CHn_CONFIG(channel), reg);
}

int ad7779_print_status(void)
{
	uint8_t tmp;

	ad7779_read_reg(AD7779_CH0_ERR_REG, &tmp);
	log_info("AD7779_CH0_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH1_ERR_REG, &tmp);
	log_info("AD7779_CH1_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH2_ERR_REG, &tmp);
	log_info("AD7779_CH2_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH3_ERR_REG, &tmp);
	log_info("AD7779_CH3_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH4_ERR_REG, &tmp);
	log_info("AD7779_CH4_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH5_ERR_REG, &tmp);
	log_info("AD7779_CH5_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH6_ERR_REG, &tmp);
	log_info("AD7779_CH6_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH7_ERR_REG, &tmp);
	log_info("AD7779_CH7_ERR_REG=0x%x", tmp);

	ad7779_read_reg(AD7779_CH0_1_SAT_ERR, &tmp);
	log_info("AD7779_CH0_1_SAT_ERR=0x%x", tmp);

	ad7779_read_reg(AD7779_CH2_3_SAT_ERR, &tmp);
	log_info("AD7779_CH2_3_SAT_ERR=0x%x", tmp);

	ad7779_read_reg(AD7779_CH4_5_SAT_ERR, &tmp);
	log_info("AD7779_CH4_5_SAT_ERR=0x%x", tmp);

	ad7779_read_reg(AD7779_CH6_7_SAT_ERR, &tmp);
	log_info("AD7779_CH6_7_SAT_ERR=0x%x", tmp);

	ad7779_read_reg(AD7779_GEN_ERR_REG_1, &tmp);
	log_info("AD7779_GEN_ERR_REG_1=0x%x", tmp);

	ad7779_read_reg(AD7779_GEN_ERR_REG_2, &tmp);
	log_info("AD7779_GEN_ERR_REG_2=0x%x", tmp);

	ad7779_read_reg(AD7779_STATUS_REG_1, &tmp);
	log_info("AD7779_STATUS_REG_1=0x%x", tmp);

	ad7779_read_reg(AD7779_STATUS_REG_2, &tmp);
	log_info("AD7779_STATUS_REG_2=0x%x", tmp);

	ad7779_read_reg(AD7779_STATUS_REG_3, &tmp);
	log_info("AD7779_STATUS_REG_3=0x%x", tmp);

	return 0;
}


static int ad7779_gpio_init(void)
{
	platformctl_t pctl;

	pctl.action = pctl_set;
	pctl.type = pctl_iomux;

	pctl.iomux.sion = 0;
	pctl.iomux.mode = 5;

	/* Configure as GPIOs */

	/* Leave /START alone, it is pulled up, as it should be */

	/* /RESET */
	pctl.iomux.mux = pctl_mux_gpio_b0_05;
	platformctl(&pctl);
	/* /CLK_SEL */
	pctl.iomux.mux = pctl_mux_gpio_b0_06;
	platformctl(&pctl);

	pctl.type = pctl_iopad;

	pctl.iopad.hys = 0;
	pctl.iopad.pus = 1;
	pctl.iopad.pue = 1;
	pctl.iopad.pke = 0;
	pctl.iopad.ode = 0;
	pctl.iopad.speed = 1;
	pctl.iopad.dse = 3;

	/* /RESET */
	pctl.iopad.pad = pctl_pad_gpio_b0_05;
	platformctl(&pctl);
	/* /CLK_SEL */
	pctl.iopad.pad = pctl_pad_gpio_b0_06;
	platformctl(&pctl);

	/* Set states */
	gpio_setPin(2, 5, 0);
	gpio_setPin(2, 6, 1);
	gpio_setDir(2, 5, 1);
	gpio_setDir(2, 6, 1);

	return AD7779_OK;
}


static int ad7779_reset(void)
{
	gpio_setPin(2, 5, 0);
	usleep(100000);
	gpio_setPin(2, 5, 1);
	usleep(100000);

	return AD7779_OK;
}


int ad7779_init(void)
{
	int res;

	while (lookup("/dev/gpio1", NULL, &ad7779_common.multidrv) < 0)
		usleep(100 * 1000);

	if ((res = lpspi_config()) < 0)
		return res;

	if ((res = ad7779_gpio_init()) < 0)
		return res;

	if ((res = ad7779_reset()) < 0)
		return res;

	if ((res = ad7779_write_reg(AD7779_CH_DISABLE, 0x00)) < 0)
		return res;

	/* Use external reference */
	if ((res = ad7779_write_reg(AD7779_ADC_MUX_CONFIG, 0x00)) < 0)
		return res;

	log_debug("switching to high resolution mode");
	if ((res = ad7779_set_mode(ad7779_mode__high_resolution)) < 0)
		return res;

	/* Use one DOUTx line; DCLK_CLK_DIV = 1 */
	log_debug("setting DOUT_FORMAT");
	if ((res = ad7779_write_reg(AD7779_DOUT_FORMAT, 0xc0)) < 0)
		return res;

	/* Make sure SRC_LOAD_SOURCE bit is cleared */
	log_debug("clearing SRC_LOAD_SOURCE bit");
	if ((res = ad7779_set_clear_bits(AD7779_SRC_UPDATE, 0, SRC_LOAD_SOURCE_BIT)) < 0)
		return res;

	log_debug("setting sampling rate");
	if ((res = ad7779_set_sampling_rate(16000)) < 0)
		return res;

	return AD7779_OK;
}
