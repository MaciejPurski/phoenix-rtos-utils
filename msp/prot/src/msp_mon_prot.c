/*
 * Library for communication between MSP430 and the host processor
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <msp_mon_prot.h>

#include <string.h>

#ifdef MSP_MON_USE_PS_UTILS
    #define PS_LOG_TAG                  "msp-mon-prot: "
    #define PS_LOG_LEVEL                0

    #include <ps_log_utils.h>
    #include <ps_buffer_utils.h>

    #define print_buffer(buf, len) do { if (PS_LOG_LEVEL <= 0) { ps_print_buffer_ex2(PS_LOG_PRIO_DBG, buf, len, 16, 1); }; } while (0)
#else
    #define log_debug(...)
    #define log_info(...)
    #define log_error(...)
    #define log_success(...)
    #define log_warn(...)

    #define print_buffer(buf, len)
#endif

#define HDLC_TERM_BYTE              (0x7e)
#define HDLC_ESC_BYTE               (0x7d)
#define HDLC_XOR_BYTE               (0x20)

#define HDCL_FCS_INIT_VAL           (0xffff)

extern uint16_t mmp_hdlc_calculate_crc16(const uint8_t* buf, uint32_t len, uint16_t base);

static int mmp_update_rx(mmp_t *mmp);
static int mmp_update_rx__receiving(mmp_t *mmp);
static int mmp_update_rx__fcs_error(mmp_t *mmp);
static int mmp_update_rx__packet_pending(mmp_t *mmp);

static int mmp_update_tx(mmp_t *mmp);
static int mmp_update_tx__sending(mmp_t *mmp);
static int mmp_update_tx__waiting_for_ack(mmp_t *mmp);

static int mmp_parse_packet(mmp_t *mmp, uint8_t *packet, uint16_t len);

static int mmp_process_packet(mmp_t *mmp, mmp_header_t *h, const uint8_t *data, uint16_t len);
static int mmp_process_ack_nack(mmp_t *mmp, mmp_header_t *h, const uint8_t *data, uint16_t len);
static int mmp_process_cmd(mmp_t *mmp, mmp_header_t *h, const uint8_t *data, uint16_t len);

static int mmp_construct_packet(mmp_t *mmp,
                                mmp_header_t *h,
                                const uint8_t *data,
                                uint16_t data_len,
                                uint8_t *out);

static int mmp_send_ack_nack(mmp_t *mmp, mmp_header_t *h, const uint8_t *resp, uint16_t resp_len);

static int mmp_hdlc_escape(const uint8_t *in, uint16_t len, uint8_t *out);

/* Warning! Packet is unescaped in-place, so input data is lost */
static int mmp_hdlc_unescape(uint8_t *data, uint16_t len);

int mmp_init(mmp_t *mmp,
             mmp_read_func_t read_byte,
             mmp_write_func_t write,
             mmp_rx_handler_t rx_handler)
{
    if (!mmp || !read_byte || !write)
        return MMP_RES__ARG_ERROR;

    mmp->tx_state = mmp_tx_state__idle;
    mmp->rx_state = mmp_rx_state__receiving;

    mmp->read_byte = read_byte;
    mmp->write = write;
    mmp->rx_handler = rx_handler;

    mmp->rx_read = 0;

    mmp->tx_seq = 0;

    mmp->initialized = 1;

    mmp_enable_tx(mmp);

    return MMP_RES__OK;
}

int mmp_update(mmp_t *mmp)
{
    int res;

    if (!mmp)
        return MMP_RES__ARG_ERROR;
    if (!mmp->initialized)
        return MMP_RES__UNINITIALIZED;

    res = mmp_update_rx(mmp);
    if (res < 0)
        return res;

    res = mmp_update_tx(mmp);
    if (res < 0)
        return res;

    return MMP_RES__OK;
}

int mmp_transmit(mmp_t *mmp,
                 uint8_t cmd,
                 const uint8_t *data,
                 uint16_t len,
                 mmp_tx_done_clbk_t clbk,
                 void *clbk_arg,
                 uint16_t timeout)
{
    int res;

    if (!mmp)
        return MMP_RES__ARG_ERROR;
    if (!mmp->initialized)
        return MMP_RES__UNINITIALIZED;
    if (!mmp->tx_enabled)
        return MMP_RES__TX_DISABLED;
    if (mmp->tx_state != mmp_tx_state__idle)
        return MMP_RES__TX_BUSY;

    mmp_header_t h;
    h.type = cmd;
    h.seq = mmp->tx_seq++;

    res = mmp_construct_packet(mmp, &h, data, len, mmp->tx_buffer);
    if (res < 0)
        return res;

    mmp->tx_packet_len = res;
    mmp->tx_sent = 0;

    mmp->tx_state = mmp_tx_state__sending_data;
    mmp->tx_done_clbk = clbk;
    mmp->tx_done_clbk_arg = clbk_arg;
    mmp->tx_header = h;

    if (timeout > 0) {
        mmp->tx_timeout = timeout;
    } else {
        mmp->tx_timeout = MMP_DEFAULT_TX_ACK_TIMEOUT;
    }

    mmp->initialized = 1;

    return MMP_RES__OK;
}

int mmp_is_ready_to_transmit(mmp_t *mmp)
{
    if (!mmp)
        return 0;
    if (!mmp->initialized)
        return 0;
    if (!mmp->tx_enabled)
        return 0;
    if (mmp->tx_state != mmp_tx_state__idle)
        return 0;

    return 1;
}

static mmp_t *mmp_default_instance = NULL;

mmp_t* mmp_get_default_instance(void)
{
    return mmp_default_instance;
}

void mmp_set_default_instance(mmp_t* mmp)
{
    mmp_default_instance = mmp;
}

void mmp_deinit(mmp_t *mmp)
{
    if (mmp->tx_state != mmp_tx_state__idle) {
        if (mmp->tx_done_clbk != NULL)
            mmp->tx_done_clbk(MMP_RES__DENINITIALIZED, NULL, 0, mmp->tx_done_clbk_arg);
    }

    mmp->initialized = 0;
}

void mmp_enable_tx(mmp_t *mmp)
{
    mmp->tx_enabled = 1;
}

void mmp_disable_tx(mmp_t *mmp)
{
    mmp->tx_enabled = 0;
}

static int mmp_update_rx(mmp_t *mmp)
{
    int res;

    do {
        switch (mmp->rx_state) {
            case mmp_rx_state__receiving:
                res = mmp_update_rx__receiving(mmp);
                break;
            case mmp_rx_state__fcs_error:
                res = mmp_update_rx__fcs_error(mmp);
                break;
            case mmp_rx_state__packet_pending:
                res = mmp_update_rx__packet_pending(mmp);
                break;
            default:
                return MMP_RES__INTERNAL_ERROR;
        }
    } while (res == MMP_RES__CONTINUE_UPDATE);

    return res;
}

static int mmp_update_rx__receiving(mmp_t *mmp)
{
    int res;

    while (1) {
        res = mmp->read_byte(mmp->rx_buffer + mmp->rx_read);
        if (res < 0) {
            log_error("Failed to read next byte (%d).", res);
            return MMP_RES__READ_FAILED;
        }
        if (res == 0)
            return MMP_RES__OK;

        if (mmp->rx_read == 0 && mmp->rx_buffer[0] != HDLC_TERM_BYTE)
            continue;

        if (mmp->rx_read != 0 && mmp->rx_buffer[mmp->rx_read] == HDLC_TERM_BYTE) {
            uint16_t packet_len = mmp->rx_read + 1;

            /* Treat the term byte in the end of the current packet as
             * a potential beginning of the next packet */
            mmp->rx_read = 1;

            res = mmp_parse_packet(mmp, mmp->rx_buffer, packet_len);
            if (res == MMP_RES__FCS_ERROR) {
                mmp->rx_state = mmp_rx_state__fcs_error;
                return MMP_RES__CONTINUE_UPDATE;
            } else if (res < 0) {
                continue; /* Parsing failed. Drop. */
            }

            log_debug("Received valid packet.");

            mmp->rx_state = mmp_rx_state__packet_pending;
            return MMP_RES__CONTINUE_UPDATE;
        }

        mmp->rx_read++;

        if (mmp->rx_read >= MMP_BUFFER_LEN)
            mmp->rx_read = 0; /* Too long to be a valid packet. Start again. */
    }
}

static int mmp_update_rx__fcs_error(mmp_t *mmp)
{
    if (mmp->tx_state != mmp_tx_state__idle && mmp->tx_state != mmp_tx_state__waiting_for_ack)
        return MMP_RES__OK; /* Won't be able to send NACK now. Try later. */

    log_debug("FCS error occurred. Replying with NACK.");

    mmp_header_t h = {MMP_CMD__NACK_FLAG, 0};
    mmp_nack_t nack;
    nack.error_code = MMP_NACK__FCS_ERROR;

    int res = mmp_send_ack_nack(mmp, &h, (uint8_t*)&nack, sizeof(nack));
    if (res < 0) {
        log_error("Failed to send NACK (in response to FCH error).");
    }

    mmp->rx_state = mmp_rx_state__receiving;

    return MMP_RES__CONTINUE_UPDATE;
}

static int mmp_update_rx__packet_pending(mmp_t *mmp)
{
    int res = mmp_process_packet(mmp, &mmp->rx_header, mmp->rx_data, mmp->rx_data_len);
    if (res == MMP_RES__TX_BUSY) {
        return MMP_RES__OK; /* Won't be able to send ACK/NACK now. Try later. */

    } else if (res == MMP_RES__OK) {
        mmp->rx_state = mmp_rx_state__receiving;
        return MMP_RES__CONTINUE_UPDATE;

    } else {
        mmp->rx_state = mmp_rx_state__receiving;
        return res;
    }
}

static int mmp_update_tx(mmp_t *mmp)
{
    int res;

    switch (mmp->tx_state) {
    case mmp_tx_state__sending_data:
    case mmp_tx_state__sending_ack:
        return mmp_update_tx__sending(mmp);

    case mmp_tx_state__waiting_for_ack:
        return mmp_update_tx__waiting_for_ack(mmp);

    case mmp_tx_state__sa_and_wfa:
        res = mmp_update_tx__sending(mmp);
        if (res < 0)
            return res;

        return mmp_update_tx__waiting_for_ack(mmp);

    default:
        return MMP_RES__OK;
    }
}

static int mmp_update_tx__sending(mmp_t *mmp)
{
    int res;

    res = mmp->write(mmp->tx_buffer + mmp->tx_sent, mmp->tx_packet_len - mmp->tx_sent);
    if (res >= 0) {
        mmp->tx_sent += res;

        if (mmp->tx_sent == mmp->tx_packet_len) {
            switch (mmp->tx_state) {
            case mmp_tx_state__sending_data:
            case mmp_tx_state__sa_and_wfa:
                mmp->tx_state = mmp_tx_state__waiting_for_ack;
                break;
            case mmp_tx_state__sending_ack:
                mmp->tx_state = mmp_tx_state__idle;
                break;
            default:
                return MMP_RES__INTERNAL_ERROR;
            }
        }
    } else {
        log_error("Failed to write data (%d).", res);
        return MMP_RES__WRITE_FAILED;
    }

    return MMP_RES__OK;
}

static int mmp_update_tx__waiting_for_ack(mmp_t *mmp)
{
    int res;

    if (mmp->tx_timeout-- <= 0) {
        switch (mmp->tx_state) {
        case mmp_tx_state__waiting_for_ack:
            mmp->tx_state = mmp_tx_state__idle;
            break;
        case mmp_tx_state__sa_and_wfa:
            mmp->tx_state = mmp_tx_state__sending_ack;
            break;
        default:
            return MMP_RES__INTERNAL_ERROR;
        }

        if (mmp->tx_done_clbk != NULL) {
            res = mmp->tx_done_clbk(MMP_RES__ACK_TIMEOUT, NULL, 0, mmp->tx_done_clbk_arg);
            if (res != MMP_RES__OK) {
                log_error("TX callback failed (%d)", res);
                return MMP_RES__TX_CLBK_ERROR;
            }
        }
    }

    return MMP_RES__OK;
}

static int mmp_parse_packet(mmp_t *mmp, uint8_t *packet, uint16_t len)
{
    int res;

    if (len < MMP_MIN_PACKET_SIZE)
        return MMP_RES__PACKET_TOO_SHORT;

    log_debug("Parsing the following:");
    print_buffer(packet, len);

    /* Discard HDLC term bytes (first and last) */
    packet++; len -= 2;

    res = mmp_hdlc_unescape(packet, len);
    if (res < 0) {
        log_error("HDLC error detected (illegal sequence).");
        return res;
    }

    len = res;

    /* Calculate FCS of whole packet except FCS bytes (last two bytes) */
    uint16_t calculated_fcs = mmp_hdlc_calculate_crc16(packet, len - 2, HDCL_FCS_INIT_VAL);

    /* Command */
    memcpy(&mmp->rx_header, packet, sizeof(mmp_header_t));
    packet += sizeof(mmp_header_t); len -= sizeof(mmp_header_t);

    /* Data */
    mmp->rx_data = packet;
    mmp->rx_data_len = len - 2;
    packet += mmp->rx_data_len;

    /* Verify FCS */
    uint16_t received_fcs = packet[0] | (packet[1] << 8);
    if (received_fcs != calculated_fcs) {
        log_error("FCS error detected.");
        return MMP_RES__FCS_ERROR;
    }

    return MMP_RES__OK;
}

static int mmp_process_packet(mmp_t *mmp, mmp_header_t *h, const uint8_t *data, uint16_t len)
{
    if (MMP_CMD__IS_ACK_NACK(h->type))
        return mmp_process_ack_nack(mmp, h, data, len);

    if (mmp->tx_state != mmp_tx_state__idle && mmp->tx_state != mmp_tx_state__waiting_for_ack)
        return MMP_RES__TX_BUSY;

    return mmp_process_cmd(mmp, h, data, len);
}

static int mmp_process_ack_nack(mmp_t *mmp, mmp_header_t *h, const uint8_t *data, uint16_t len)
{
    int res = MMP_RES__OK;

    if (mmp->tx_state == mmp_tx_state__waiting_for_ack) {
        mmp->tx_state = mmp_tx_state__idle;
    } else if (mmp->tx_state == mmp_tx_state__sa_and_wfa) {
        mmp->tx_state = mmp_tx_state__sending_ack;
    } else {
        log_warn("Received unexpected ACK/NACK (possibly for timed-out packet).");
        return MMP_RES__OK;
    }

    if (MMP_CMD__CLEAR_ACK_NACK(h->type) != mmp->tx_header.type) {
        res = MMP_RES__ACK_NACK_CMD_MISMATCH;
    } else if (h->seq != mmp->tx_header.seq) {
        res = MMP_RES__ACK_NACK_SEQ_MISMATCH;
    } else if (h->type & MMP_CMD__NACK_FLAG) {
        res = MMP_RES__NACK;
    }

    if (mmp->tx_done_clbk) {
        res = mmp->tx_done_clbk(res, data, len, mmp->tx_done_clbk_arg);
        if (res != MMP_RES__OK) {
            log_error("TX callback failed (%d)", res);
            return MMP_RES__TX_CLBK_ERROR;
        }
    }

    return MMP_RES__OK;
}

static int mmp_process_cmd(mmp_t *mmp, mmp_header_t *h, const uint8_t *data, uint16_t len)
{
    int res;

    if (!mmp->rx_handler) {
        log_error("No handler for received packets (NULL pointer).");
        return MMP_RES__MISSING_RX_HANDLER;
    }

    uint8_t resp[MMP_MAX_PAYLOAD_LEN];
    uint16_t resp_len = sizeof(resp);
    mmp_header_t header = *h;

    res = mmp->rx_handler(h->type, data, len, resp, &resp_len);
    if (res == MMP_RES__OK) {
        header.type |= MMP_CMD__ACK_FLAG;
    } else if (res == MMP_RES__NACK) {
        header.type |= MMP_CMD__NACK_FLAG;
    } else {
        log_error("RX handler failed (%d).", res);
        return MMP_RES__RX_HANDLER_FAILED;
    }

    return mmp_send_ack_nack(mmp, &header, resp, resp_len);
}

static int mmp_construct_packet(mmp_t *mmp,
                                mmp_header_t *h,
                                const uint8_t *data,
                                uint16_t data_len,
                                uint8_t *out)
{
    uint8_t *orig_out = out;
    uint16_t fcs = HDCL_FCS_INIT_VAL;

    if (data_len > MMP_MAX_PAYLOAD_LEN)
        return MMP_RES__PAYLOAD_TOO_LONG;
    if (!mmp)
        return MMP_RES__ARG_ERROR;

    *out++ = HDLC_TERM_BYTE;

    out += mmp_hdlc_escape((uint8_t*)h, sizeof(mmp_header_t), out);
    fcs = mmp_hdlc_calculate_crc16((uint8_t*)h, sizeof(mmp_header_t), fcs);

    if (data && data_len > 0) {
        out += mmp_hdlc_escape(data, data_len, out);
        fcs = mmp_hdlc_calculate_crc16(data, data_len, fcs);
    }

    uint8_t _fcs[2] = {fcs & 0xff, (fcs >> 8) & 0xff};
    out += mmp_hdlc_escape(_fcs, 2, out);

    *out++ = HDLC_TERM_BYTE;

    log_debug("Constructed packet:");
    print_buffer(orig_out, out - orig_out);

    return out - orig_out;
}

static int mmp_send_ack_nack(mmp_t *mmp, mmp_header_t *h, const uint8_t *resp, uint16_t resp_len)
{
    int res;

    res = mmp_construct_packet(mmp, h, resp, resp_len, mmp->tx_buffer);
    if (res < 0)
        return res;

    mmp->tx_packet_len = res;
    mmp->tx_sent = 0;

    if (mmp->tx_state == mmp_tx_state__idle) {
        mmp->tx_state = mmp_tx_state__sending_ack;
    } else if (mmp->tx_state == mmp_tx_state__waiting_for_ack) {
        mmp->tx_state = mmp_tx_state__sa_and_wfa;
    } else {
        return MMP_RES__INTERNAL_ERROR;
    }

    return MMP_RES__OK;
}

static int mmp_hdlc_escape(const uint8_t *in, uint16_t len, uint8_t *out)
{
    uint8_t *orig_out = out;

    if (!in || !out || len == 0)
        return 0;

    while (len--) {
        uint8_t c = *in++;
        if (c == HDLC_TERM_BYTE || c == HDLC_ESC_BYTE) {
            *out++ = HDLC_ESC_BYTE;
            *out++ = c ^ HDLC_XOR_BYTE;
        } else {
            *out++ = c;
        }
    }

    return out - orig_out;
}

static int mmp_hdlc_unescape(uint8_t *data, uint16_t len)
{
    uint8_t *in = data, *out = data;

    while (len--) {
        uint8_t c = *in++;
        if (c == HDLC_ESC_BYTE) {
            if (len == 0)
                return MMP_RES__HDLC_ERROR;
            *out++ = *in++ ^ HDLC_XOR_BYTE;
            len--;
        } else {
            *out++ = c;
        }
    }

    return out - data;
}
