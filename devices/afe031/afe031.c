#include "afe03x.h"

#include <string.h>
#include <errno.h>

#define COL_RED     "\033[1;31m"
#define COL_CYAN    "\033[1;36m"
#define COL_NORMAL  "\033[0m"

#define LOG_TAG "afe031 : "
#define log_debug(fmt, ...)     do { printf(LOG_TAG fmt "\n", ##__VA_ARGS__); } while (0)
#define log_info(fmt, ...)      do { printf(COL_CYAN LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)
#define log_error(fmt, ...)     do { printf(COL_RED  LOG_TAG fmt "\n" COL_NORMAL, ##__VA_ARGS__); } while (0)

#define SPI_IDX          0
#define SPI_CHIP_SELECT  0

#define PS_SPI_MODE(spi_idx, config_idx, chip_select_mask, keep_selected)    \
        ((((uint32_t)(uint8_t)spi_idx)         <<  0)  | \
         (((uint32_t)(uint8_t)config_idx)      <<  8)  | \
         (((uint32_t)(uint8_t)chip_select_mask)<<  16) | \
         (((uint32_t)(uint8_t)keep_selected)   <<  24) )

#define PS_SPI_MODE0(chip_select_mask, keep_selected)  PS_SPI_MODE(0, 0, chip_select_mask, keep_selected)

#define PS_SPI_MODE__SPI_IDX(spi_mode)       ((uint8_t)((spi_mode) >>  0))
#define PS_SPI_MODE__CONFIG_IDX(spi_mode)    ((uint8_t)((spi_mode) >>  8))
#define PS_SPI_MODE__CHIP_MASK(spi_mode)     ((uint8_t)((spi_mode) >> 16))
#define PS_SPI_MODE__KEEP_SELECTED(spi_mode) ((uint8_t)((spi_mode) >> 24))

#define SPI_CMD_CONFIG_IDX   0x0
#define SPI_CMD_MODE  PS_SPI_MODE(SPI_IDX, SPI_CMD_CONFIG_IDX, SPI_CHIP_SELECT, 0)

#define SPI_DAC_CONFIG_IDX   0x0
#define SPI_DAC_MODE  PS_SPI_MODE(SPI_IDX, SPI_DAC_CONFIG_IDX, SPI_CHIP_SELECT, 0)

#define REG_ENABLE1     0x1
#define REG_GAIN_SELECT 0x2
#define REG_ENABLE2     0x3
#define REG_CONTROL1    0x4
#define REG_CONTROL2    0x5
#define REG_RESET       0x9
#define REG_DIE_ID      0xa
#define REG_REVISION    0xb

#define REG_ENABLE1_PA     (1 << 0)
#define REG_ENABLE1_TX     (1 << 1)
#define REG_ENABLE1_RX     (1 << 2)
#define REG_ENABLE1_ERX    (1 << 3)
#define REG_ENABLE1_ETX    (1 << 4)
#define REG_ENABLE1_DAC    (1 << 5)

#define REG_GAIN_SELECT_RX1(x) (((x) & 0x3) << 0) // 1/4, 1/2, 1, 2
#define REG_GAIN_SELECT_RX2(x) (((x) & 0x3) << 2) // 1, 4, 16, 64
#define REG_GAIN_SELECT_TX(x)  (((x) & 0x3) << 4) // 0.25, 0.5, 0.7, 1
#define REG_GAIN_SELECT_RX1_MASK  REG_GAIN_SELECT_RX1(0xff)
#define REG_GAIN_SELECT_RX2_MASK  REG_GAIN_SELECT_RX2(0xff)
#define REG_GAIN_SELECT_TX_MASK   REG_GAIN_SELECT_TX(0xff)

#define REG_ENABLE2_ZC     (1 << 0)
#define REG_ENABLE2_REF1   (1 << 1)
#define REG_ENABLE2_REF2   (1 << 2)
#define REG_ENABLE2_PA_OUT (1 << 3)

#define REG_CONTROL1_TX_CAL     (1 << 0)
#define REG_CONTROL1_RX_CAL     (1 << 1)
#define REG_CONTROL1_TX_PGA_CAL (1 << 2)
#define REG_CONTROL1_CA_CBCD    (1 << 3)
#define REG_CONTROL1_TX_FLAG    (1 << 6)
#define REG_CONTROL1_RX_FLAG    (1 << 7)

#define REG_CONTROL2_T_FLAG_EN  (1 << 5)
#define REG_CONTROL2_I_FLAG_EN  (1 << 6)

#define REG_RESET_SOFTRST       ((0x5) << 2) // 101b performs software reset
#define REG_RESET_T_FLAG        (1 << 5)
#define REG_RESET_I_FLAG        (1 << 6)

#define AFE_CMD(read, reg_addr, value)  ( ((read) << 15) | ((reg_addr) << 8) | ((value) << 0) )
#define AFE_READ_CMD(reg_addr)  AFE_CMD(1, reg_addr, 0)
#define AFE_WRITE_CMD(reg_addr, value)  AFE_CMD(0, reg_addr, value)

#define AFE_CMD_ADDR_BITS(cmd)  ((cmd) & 0xff00)
#define AFE_CMD_VALUE_BITS(cmd) ((cmd) & 0x00ff)

#define EXPECTED_DIE_ID     0x0
#define EXPECTED_REVISION   0x2

static int (*s_spi_exchange)(const uint8_t *out, uint8_t *in, uint16_t len);

static void spi_send16(uint32_t spi_mode, uint16_t _out)
{
    uint8_t in[2], out[2];

	out[0] = _out >> 8;
	out[1] = _out;

	s_spi_exchange(out, in, sizeof(out));
}

static uint16_t spi_recv16(uint8_t spi_idx)
{
    uint8_t in[2], out[2];

    memset(out, 0, sizeof(out));

    s_spi_exchange(out, in, sizeof(out));

    uint16_t _in = in[1] | ((uint16_t)in[0] << 8);

    return _in;
}

static void afe031_write_cmd(uint16_t cmd)
{
    spi_send16(SPI_CMD_MODE, cmd);
    spi_recv16(SPI_IDX);
}

static void afe031_write_reg(uint8_t reg_addr, uint8_t val)
{
    afe031_write_cmd(AFE_WRITE_CMD(reg_addr, val));
}

static uint8_t afe031_read_reg(uint8_t reg_addr)
{
    uint32_t cnt = 0;
    const uint16_t cmd = AFE_READ_CMD(reg_addr);
    spi_send16(SPI_CMD_MODE, cmd); // send read request
    spi_recv16(SPI_IDX); // ignore old afe031 register value
    for (;;) {
        spi_send16(SPI_CMD_MODE, cmd); // send read request
        uint16_t res = spi_recv16(SPI_IDX);
        //if (res != 0)
		log_debug("sent : 0x%04x   recv : 0x%04x", cmd, res);
        if (AFE_CMD_ADDR_BITS(cmd) == AFE_CMD_ADDR_BITS(res)) { // response to our request, or some old garbage?
			log_debug("afe031_read_reg : returning 0x%x", AFE_CMD_VALUE_BITS(res));
            return AFE_CMD_VALUE_BITS(res);
        }
        cnt += 1;
        // Should be ready immediately - if not, then the afe is broken.
        // But to be absolutely sure (before giving up completely), just check a few more times.
		if (cnt > 10)
            return 0x00;
    }
    return 0;
}

static volatile uint8_t  s_last_gain_reg_status = 0x0;

int ps_afe03x_init(ps_afe03x_cfg_t *cfg)
{
    s_spi_exchange = cfg->spi_exchange;

	const uint8_t reset_bits = REG_RESET_T_FLAG | REG_RESET_I_FLAG | REG_RESET_SOFTRST;
	afe031_write_reg(REG_RESET, reset_bits);

	uint32_t retry_cnt = 0;
	for (;;) {
		const uint8_t revision = afe031_read_reg(REG_REVISION);
		const uint8_t die_id   = afe031_read_reg(REG_DIE_ID);
		log_debug("die_id=0x%02x, revision=0x%02x", die_id, revision);

		if (die_id == EXPECTED_DIE_ID && revision == EXPECTED_REVISION)
			break;

		retry_cnt += 1;
		log_error("Reading dev-id failed : afe is BROKEN");
		return -1;
	}

	// RX gain = 0.25, TX gain = 1.0
	const uint8_t gain_bits = REG_GAIN_SELECT_RX1(0x0) | REG_GAIN_SELECT_RX2(0x0) | REG_GAIN_SELECT_TX(0x3);
	afe031_write_reg(REG_GAIN_SELECT, gain_bits);

	s_last_gain_reg_status = gain_bits;

	const uint8_t enable1_bits = REG_ENABLE1_RX | REG_ENABLE1_TX | REG_ENABLE1_PA; // We don't use at all: (ERX, ETX, DAC)
	afe031_write_reg(REG_ENABLE1, enable1_bits);

	const uint8_t enable2_bits = REG_ENABLE2_REF1 | REG_ENABLE2_REF2; // disable: REG_ENABLE2_PA_OUT
	afe031_write_reg(REG_ENABLE2, enable2_bits);

	const uint8_t control1_bits = 0x0; // disabled: REG_CONTROL1_CA_CBCD (use CENELEC-A), calibration bits
	afe031_write_reg(REG_CONTROL1, control1_bits);

	const uint8_t control2_bits = REG_CONTROL2_T_FLAG_EN | REG_CONTROL2_I_FLAG_EN; // enable over-I/T flags in "reset" register
	afe031_write_reg(REG_CONTROL2, control2_bits);

	return 0;
}

uint16_t ps_afe03x_switch_rx_gain_cmd(uint8_t val)
{
    #define RXBITS(rx1, rx2)  (REG_GAIN_SELECT_RX1(rx1) | REG_GAIN_SELECT_RX2(rx2))
    static const uint8_t s_rx_bits[10] = {
            RXBITS(0, 0), RXBITS(1, 0),
            RXBITS(2, 0), RXBITS(3, 0),
            RXBITS(2, 1), RXBITS(3, 1),
            RXBITS(2, 2), RXBITS(3, 2),
            RXBITS(2, 3), RXBITS(3, 3),
    };
    #undef RXBITS

    // only using RX2
    uint8_t new_status = s_last_gain_reg_status & ~(REG_GAIN_SELECT_RX1_MASK | REG_GAIN_SELECT_RX2_MASK);
    new_status |= s_rx_bits[val];
    s_last_gain_reg_status = new_status;

    return AFE_WRITE_CMD(REG_GAIN_SELECT, new_status);
}

int ps_afe03x_switch_rx_gain(uint8_t val)
{
    /* TODO: implement write verification */
    afe031_write_cmd(ps_afe03x_switch_rx_gain_cmd(val));
    return 0;
}

int ps_afe03x_switch_tx_gain(uint8_t val)
{
    uint8_t new_status = s_last_gain_reg_status & ~REG_GAIN_SELECT_TX_MASK;
    new_status |= REG_GAIN_SELECT_TX(val);
    s_last_gain_reg_status = new_status;

    /* TODO: implement write verification */
    afe031_write_reg(REG_GAIN_SELECT, new_status);
    return 0;
}

#if AFE03x_OVERCURRENT_VALID_ONLY_IN_TX
static uint8_t s_tx_enabled = 0;
static uint8_t s_last_valid_i_flag = 0;

static void store_i_flag(void)
{
    const uint8_t reg = afe031_read_reg(REG_RESET);
    s_last_valid_i_flag = (reg & REG_RESET_I_FLAG) != 0;
}
static void clear_i_flag(void)
{
    const uint8_t reg = afe031_read_reg(REG_RESET);
    uint8_t new_reg = reg & ~(REG_RESET_I_FLAG);
    afe031_write_reg(REG_RESET, new_reg);
    s_last_valid_i_flag = 0;
}
#endif

uint16_t ps_afe03x_switch_to_rx_cmd(void)
{
    // clear bit: REG_ENABLE2_PA_OUT
    const uint8_t enable2_bits = REG_ENABLE2_REF1 | REG_ENABLE2_REF2;
    return AFE_WRITE_CMD(REG_ENABLE2, enable2_bits);
}

int ps_afe03x_switch_to_rx(void)
{
#if AFE03x_OVERCURRENT_VALID_ONLY_IN_TX
    if (!s_tx_enabled)
        return 0;
    store_i_flag();
    s_tx_enabled = 0;
#endif

    afe031_write_cmd(ps_afe03x_switch_to_rx_cmd());
    return 0;
}

uint16_t ps_afe03x_switch_to_tx_cmd(void)
{
    // set bit: REG_ENABLE2_PA_OUT
    const uint8_t enable2_bits = REG_ENABLE2_REF1 | REG_ENABLE2_REF2 | REG_ENABLE2_PA_OUT;
    return AFE_WRITE_CMD(REG_ENABLE2, enable2_bits);
}

int ps_afe03x_switch_to_tx(void)
{
    afe031_write_cmd(ps_afe03x_switch_to_tx_cmd());

#if AFE03x_OVERCURRENT_VALID_ONLY_IN_TX
    s_tx_enabled = 1;
    clear_i_flag();
#endif

    return 0;
}

int ps_afe03x_switch_to_rx_async(void)
{
    /* FIXME: Implement ps_afe03x_switch_to_rx_async  */
    return -ENOSYS;
}

int ps_afe03x_switch_to_tx_async(uint32_t *wait_us)
{
    /* FIXME: Implement ps_afe03x_switch_to_tx_async  */
    return -ENOSYS;
}

void ps_afe03x_enable_dac(void)
{
    uint8_t reg;

    reg = afe031_read_reg(REG_ENABLE1);
//    log_debug("read REG_ENABLE1 = 0x%x", reg);
    reg |= REG_ENABLE1_DAC;
//    log_debug("setting REG_ENABLE1 = 0x%x", reg);
    afe031_write_reg(REG_ENABLE1, reg);
}

void ps_afe03x_disable_dac(void)
{
    uint8_t reg;

    reg = afe031_read_reg(REG_ENABLE1);
    //log_debug("read REG_ENABLE1 = 0x%x", reg);
    reg &= ~REG_ENABLE1_DAC;
    //log_debug("setting REG_ENABLE1 = 0x%x", reg);
    afe031_write_reg(REG_ENABLE1, reg);
}

int ps_afe03x_print_status(void)
{
    /* FIXME: Implement ps_afe03x_print_status  */
    return -ENOSYS;
}

uint16_t ps_afe03x_nop_cmd(void)
{
    return AFE_READ_CMD(REG_DIE_ID);
}
