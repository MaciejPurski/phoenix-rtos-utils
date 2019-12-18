#ifndef PS_AFE03x_H
#define PS_AFE03x_H

#include <stdint.h>
#include <stdio.h>

typedef enum {
    ps_afe03x_band__CENELEC_A = 0,
    ps_afe03x_band__FCC       = 1,
} ps_afe03x_band_t;

typedef struct {
    int (*spi_exchange)(const uint8_t *out, uint8_t *in, uint16_t len);
    int (*spi_transmit_non_block)(const uint8_t *out, uint16_t len);

    ps_afe03x_band_t band;

    /* When set, oversampling block (by 4) in AFE is enabled */
    int oversampling_enabled;

    /* When set, the AFE SPI slave latches the first 12 bits and forwards
     * them to the DSP path. The remaining four bits are dropped.
     * Otherwise only 12 bits can be sent. */
    int use_16bit_envelope;

    /* Frequency at which samples are sent to the AFE (it doesn't take AFE's
     * internal oversampling into account) */
    uint32_t sampling_freq;

    /* Should be divisible by sampling frequency */
    uint32_t xclk_freq;
} ps_afe03x_cfg_t;

extern int ps_afe03x_init(ps_afe03x_cfg_t *cfg);

extern uint16_t ps_afe03x_switch_rx_gain_cmd(uint8_t val);
extern uint16_t ps_afe03x_switch_to_rx_cmd(void);
extern uint16_t ps_afe03x_switch_to_tx_cmd(void);

extern int ps_afe03x_switch_rx_gain(uint8_t val);
extern int ps_afe03x_switch_tx_gain(uint8_t val);

extern int ps_afe03x_switch_to_rx(void);
extern int ps_afe03x_switch_to_tx(void);

/* Safe to be called from ISR version of ps_afe03x_switch_to_rx.
 * Doesn't use any syscalls, doesn't block for a long time (no longer than a few
 * SPI transactions). */
extern int ps_afe03x_switch_to_rx_async(void);

/* Safe to be called from ISR version of ps_afe03x_switch_to_tx.
 * Doesn't use any syscalls, doesn't block for a long time (no longer than a few
 * SPI transactions).
 * In some cases (say afe032) switch to tx consists of multiple, spaced in time
 * stages. To avoid blocking for a long time (sleeping or busy waiting), it may
 * return -EAGAIN, additionally passing minimum time before the next call
 * through parameter. */
extern int ps_afe03x_switch_to_tx_async(uint32_t *wait_us);

extern void ps_afe03x_disable_dac(void);
extern void ps_afe03x_enable_dac(void);

extern int ps_afe03x_print_status(void);

extern uint16_t ps_afe03x_nop_cmd(void);

#endif /* PS_AFE03x_H */
