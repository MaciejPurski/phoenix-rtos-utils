/*
 * MSP430 BSL programming library
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include "../include/bsl.h"
#include "../include/bsl_defs.h"
#include "../hal/hal.h"

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <log.h>

static int bsl_send_packet(const uint8_t *data, uint16_t len, int check_response);
static int bsl_read_packet(uint8_t *data, uint16_t expected_len);
static int bsl_get_ack_nack(void);
static int bsl_check_response(void);
static int bsl_read_block(uint32_t addr, uint8_t *data, uint16_t len);
static uint16_t bsl_calculate_crc(const uint8_t *data, uint16_t len);

static const uint8_t bsl_default_password[BSL_MSG_PASSWORD_LEN] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

int bsl_open(const char *device,
             const uint8_t* password,
             int no_entry_seq,
             int invert_tst,
             int invert_rst)
{
    int res;

    res = bsl_hal_init();
    if (res < 0) {
        log_error("Failed to initialize BSL HAL.");
        return res;
    }

    res = bsl_hal_serial_open(device);
    if (res < 0) {
        log_error("Failed to open serial.");
        return res;
    }

    if (!no_entry_seq) {
        res = bsl_entry_sequence(invert_tst, invert_rst);
        if (res < 0) {
            log_error("Failed to generate BSL entry sequence.");
            return res;
        }
    }

    if (!password) {
        password = bsl_default_password;

        res = bsl_mass_erase();
        if (res < 0) {
            log_error("Mass erase failed.");
            return res;
        }
    }

    res = bsl_unlock(password);
    if (res < 0) {
        log_error("Failed to unlock the BSL.");
        return res;
    }

    return BSL_RES_OK;
}

int bsl_entry_sequence(int invert_tst, int invert_rst)
{
    int res;

    if ((res = bsl_hal_set_tst_state(1, invert_tst)) < 0) return res;
    if ((res = bsl_hal_set_rst_state(1, invert_rst)) < 0) return res;

    bsl_hal_sleep_ms(500);
    if ((res = bsl_hal_set_tst_state(0, invert_tst)) < 0) return res;
    if ((res = bsl_hal_set_rst_state(0, invert_rst)) < 0) return res;

    bsl_hal_sleep_ms(1);
    if ((res = bsl_hal_set_tst_state(1, invert_tst)) < 0) return res;

    bsl_hal_sleep_ms(1);
    if ((res = bsl_hal_set_tst_state(0, invert_tst)) < 0) return res;

    bsl_hal_sleep_ms(1);
    if ((res = bsl_hal_set_tst_state(1, invert_tst)) < 0) return res;

    bsl_hal_sleep_ms(1);
    if ((res = bsl_hal_set_rst_state(1, invert_rst)) < 0) return res;

    bsl_hal_sleep_ms(1);
    if ((res = bsl_hal_set_tst_state(0, invert_tst)) < 0) return res;

    bsl_hal_sleep_ms(100);

    return BSL_RES_OK;
}

int bsl_unlock(const uint8_t *password)
{
    int res;
    uint8_t command[BSL_MSG_PASSWORD_LEN + 1];

    log_debug("Sending password:");
    print_buffer(password, BSL_MSG_PASSWORD_LEN);

    /* Send password */
    command[0] = BSL_CMD_RX_PASSWORD;
    memcpy(command + 1, password, BSL_MSG_PASSWORD_LEN);
    res = bsl_send_packet(command, sizeof(command), 1);
    if (res != 0)
        return res;

    log_debug("BSL unlocked.");

    return BSL_RES_OK;
}

int bsl_mass_erase(void)
{
    int res;
    uint8_t command;

    /* Command mass erase */
    command = BSL_CMD_MASS_ERASE;
    res = bsl_send_packet(&command, 1, 1);
    if (res != 0)
        return res;

    log_debug("Mass erase done.");

    return BSL_RES_OK;
}

int bsl_erase_check(uint32_t addr, uint16_t len)
{
    int res, i;
    uint8_t buffer[len];

    res = bsl_read_block(addr, buffer, len);
    if (res < 0)
        return res;

    for (i = 0; i < len; i++) {
        if (buffer[i] != 0xff) {
            log_error("Erase check failed (addr=0x%02x, val=%0x%02x", addr + i, buffer[i]);
            return BSL_RES_ERASE_CHECK_FAILED;
        }
    }

    log_debug("Erase check passed.");

    return BSL_RES_OK;
}

int bsl_program(uint32_t addr, const uint8_t *data, uint16_t len)
{
    int res;
    uint8_t command[len + 4];

    command[0] = BSL_CMD_RX_DATA_BLOCK;
    command[1] = addr & 0xff;
    command[2] = (addr >> 8) & 0xff;
    command[3] = (addr >> 16) & 0xff;
    memcpy(command + 4, data, len);

    res = bsl_send_packet(command, sizeof(command), 1);
    if (res != 0)
        return res;

    log_debug("Segment written successfully.");

    return BSL_RES_OK;
}

int bsl_verify(uint32_t addr, const uint8_t *data, uint16_t len)
{
    int res;
    uint8_t buffer[len];

    res = bsl_read_block(addr, buffer, len);
    if (res < 0)
        return res;

    if (memcmp(data, buffer, len) != 0)
        return BSL_RES_VERIFICATION_FAILED;

    log_debug("Segment verified successfully.");

    return BSL_RES_OK;
}

int bsl_load_pc(uint32_t addr)
{
    int res;
    uint8_t command[4];

    command[0] = BSL_CMD_LOAD_PC;
    command[1] = addr & 0xff;
    command[2] = (addr >> 8) & 0xff;
    command[3] = (addr >> 16) & 0xff;

    res = bsl_send_packet(command, sizeof(command), 1);
    if (res != 0)
        return res;

    return BSL_RES_OK;
}

int bsl_close(void)
{
    return bsl_hal_serial_close();
}

static int bsl_send_packet(const uint8_t *data, uint16_t len, int check_response)
{
    int res;
    uint8_t buffer[len + 5];

    if (!data)
        return BSL_RES_ARG_ERROR;

    uint16_t crc = bsl_calculate_crc(data, len);

    buffer[0] = 0x80;
    buffer[1] = len & 0xff;
    buffer[2] = (len >> 8) & 0xff;
    memcpy(buffer + 3, data, len);
    buffer[len + 3] = crc & 0xff;
    buffer[len + 4] = (crc >> 8) & 0xff;

    log_debug("Sending packet:");
    print_buffer(buffer, sizeof(buffer));

    /* Send packet */
    res = bsl_hal_serial_write(buffer, sizeof(buffer));
    if (res < 0) {
        log_error("Failed to send packet.");
        return res;
    }

    res = bsl_get_ack_nack();
    if (res < 0)
        return res;

    if (check_response) {
        res = bsl_check_response();
        if (res < 0)
            return res;
    }

    return BSL_RES_OK;
}

static int bsl_read_packet(uint8_t *data, uint16_t expected_len)
{
    int res;
    uint8_t buffer[2];

    if (!data)
        return BSL_RES_ARG_ERROR;

    /* Find packet header (0x80) */
    /* TODO: Timeout*/
    do {
        res = bsl_hal_serial_read(buffer, 1, BSL_READ_TIMEOUT_MS);
        if (res < 0) {
            log_error("Failed to read header (%d)", res);
            return res;
        }
    } while (buffer[0] != 0x80);

    /* Read packet's length */
    res = bsl_hal_serial_read(buffer, 2, BSL_READ_TIMEOUT_MS);
    if (res < 0) {
        log_error("Failed to read payload length (%d)", res);
        return res;
    }
    uint16_t length = buffer[0] | ((uint16_t)buffer[1] << 8);
    if (length != expected_len) {
        log_error("Packet length other than expected (received: %u, expected: %u).", length, expected_len);
        return BSL_RES_UNEXPECTED_LENGTH;
    }

    log_debug("Payload length: 0x%02x (%u)", length, length);

    /* Read payload */
    res = bsl_hal_serial_read(data, expected_len, BSL_READ_TIMEOUT_MS);
    if (res < 0) {
        log_error("Failed to read payload (%d)", res);
        return res;
    }

    log_debug("Received payload:");
    print_buffer(data, expected_len);

    /* Read CRC */
    res = bsl_hal_serial_read(buffer, 2, BSL_READ_TIMEOUT_MS);
    if (res < 0) {
        log_error("Failed to read CRC (%d)", res);
        return res;
    }
    uint16_t received_crc = buffer[0] | ((uint16_t)buffer[1] << 8);

    /* Verify CRC */
    uint16_t calculated_crc = bsl_calculate_crc(data, expected_len);
    if (received_crc != calculated_crc) {
        log_error("CRC error (received = 0x%02x, calculated = 0x%02x).",
            received_crc, calculated_crc);
        return BSL_RES_CRC_ERROR;
    }

    return BSL_RES_OK;
}

static int bsl_get_ack_nack(void)
{
    int res;
    uint8_t ack_nack;

    /* Get ACK (or error message) */
    res = bsl_hal_serial_read(&ack_nack, 1, BSL_READ_TIMEOUT_MS);
    if (res < 0) {
        log_error("Failed to read response (%d)", res);
        return res;
    }

    /* Check ACK/NACK */
    if (ack_nack != BSL_UART_ACK) {
        log_error("No ACK (0x%02x).", ack_nack);
        return BSL_RES_NO_ACK;
    } else {
        log_debug("ACK received.");
    }

    return BSL_RES_OK;
}

static int bsl_check_response(void)
{
    int res;
    uint8_t buffer[2];

    /* Read response */
    log_debug("Reading response...");
    res = bsl_read_packet(buffer, 2);
    if (res != 0)
        return res;

    /* Check response */
    if (buffer[0] != BSL_RESP_MESSAGE_REPLY) {
        log_error("Unexpected response CMD (0x%02x)", buffer[0]);
        return BSL_RES_UNEXPECTED_CMD;

    } else if (buffer[1] != BSL_MSG_SUCCESS) {
        log_error("Unexpected response message (0x%02x - %s)",
            buffer[1], bsl_msg_to_string(buffer[1]));
        return BSL_RES_UNEXPECTED_MSG;
    }

    return BSL_RES_OK;
}

static int bsl_read_block(uint32_t addr, uint8_t *data, uint16_t len)
{
    int res;

    uint8_t command[6];
    command[0] = BSL_CMD_TX_DATA_BLOCK;
    command[1] = addr & 0xff;
    command[2] = (addr >> 8) & 0xff;
    command[3] = (addr >> 16) & 0xff;
    command[4] = len & 0xff;
    command[5] = (len >> 8) & 0xff;

    log_debug("Requesting data block...");
    res = bsl_send_packet(command, sizeof(command), 0);
    if (res != 0)
        return res;

    log_debug("Reading response...");
    uint8_t buffer[len + 1];
    res = bsl_read_packet(buffer, len + 1);
    if (res != 0)
        return res;

    /* Check response CMD */
    if (buffer[0] != BSL_RESP_DATA_REPLY) {
        log_error("Unexpected response CMD (0x%02x)", buffer[0]);
        return BSL_RES_UNEXPECTED_CMD;
    }

    memcpy(data, buffer + 1, len);

    return BSL_RES_OK;
}

static uint16_t bsl_calculate_crc(const uint8_t *data, uint16_t len)
{
    uint16_t i, crc = 0xffff;

    for (i = 0; i < len; i++) {
        uint16_t x = ((crc >> 8) ^ data[i]) & 0xff;
        x ^= x >> 4;
        crc = ((crc << 8) ^ (x << 12) ^ (x << 5) ^ x) & 0xffff;
    }

    return crc;
}
