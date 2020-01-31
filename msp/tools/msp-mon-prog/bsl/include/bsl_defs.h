/*
 * MSP430 BSL related definitions
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP430_BSL_PROG_BSL_DEFS_H
#define MSP430_BSL_PROG_BSL_DEFS_H

/* Return values */
#define BSL_RES_OK                          (0)
#define BSL_RES_TIMEOUT                     (-1)

#define BSL_RES_ERROR                       (-2) /* Unspecified error */

#define BSL_RES_CRC_ERROR                   (-3)
#define BSL_RES_UNEXPECTED_CMD              (-4)
#define BSL_RES_UNEXPECTED_MSG              (-5)
#define BSL_RES_NO_ACK                      (-6)
#define BSL_RES_UNEXPECTED_LENGTH           (-7)
#define BSL_RES_ARG_ERROR                   (-10)
#define BSL_RES_WRONG_PASSWORD              (-11)
#define BSL_RES_ERASE_CHECK_FAILED          (-12)
#define BSL_RES_VERIFICATION_FAILED         (-13)

#define BSL_RES_SERIAL_INIT_ERROR           (-14)
#define BSL_RES_SERIAL_CLOSED               (-15)
#define BSL_RES_SERIAL_IO_ERROR             (-16)
#define BSL_RES_SERIAL_CLOSE_ERROR          (-17)

/* BSL Core Commands */
#define BSL_CMD_RX_DATA_BLOCK               (0x10)
#define BSL_CMD_RX_PASSWORD                 (0x11)
#define BSL_CMD_ERASE_SEGMENT               (0x12)
#define BSL_CMD_TOGGLE_INFO                 (0x13)
#define BSL_CMD_ERASE_BLOCK                 (0x14)
#define BSL_CMD_MASS_ERASE                  (0x15)
#define BSL_CMD_CRC_CHECK                   (0x16)
#define BSL_CMD_LOAD_PC                     (0x17)
#define BSL_CMD_TX_DATA_BLOCK               (0x18)
#define BSL_CMD_TX_BSL_VERSION              (0x19)
#define BSL_CMD_TX_BUFFER_SIZE              (0x1A)
#define BSL_CMD_RX_DATA_BLOCK_FAST          (0x1B)

/* UART Error Messages */
#define BSL_UART_ACK                        (0x00)
#define BSL_UART_HEADER_INCORRECT           (0x51)
#define BSL_UART_CHECKSUM_INCORRECT         (0x52)
#define BSL_UART_PACKET_SIZE_ZERO           (0x53)
#define BSL_UART_PACKET_TOO_BIG             (0x54)
#define BSL_UART_UNKNOWN_ERROR              (0x55)
#define BSL_UART_UNKNOWN_BAUD_RATE          (0x56)

/* BSL Core Responses */
#define BSL_RESP_DATA_REPLY                 (0x3A)
#define BSL_RESP_MESSAGE_REPLY              (0x3B)

/* BSL Core Messages */
#define BSL_MSG_SUCCESS                     (0x00)
#define BSL_MSG_WRITE_CHECK_FAILED          (0x01)
#define BSL_MSG_FLASH_FAIL_BIT_SET          (0x02)
#define BSL_MSG_VOLTAGE_CHANGE              (0x03)
#define BSL_MSG_LOCKED                      (0x04)
#define BSL_MSG_PASSWORD_ERROR              (0x05)
#define BSL_MSG_BYTE_WRITE_FORBIDDEN        (0x06)
#define BSL_MSG_UNKNOWN_COMMAND             (0x07)
#define BSL_MSG_PACKET_TOO_BIG              (0x08)
#define BSL_MSG_INVALID_APP                 (0x09)

static inline const char* bsl_msg_to_string(int msg)
{
    switch (msg) {
    case BSL_MSG_SUCCESS:
        return "BSL_MSG_SUCCESS";
    case BSL_MSG_WRITE_CHECK_FAILED:
        return "BSL_MSG_WRITE_CHECK_FAILED";
    case BSL_MSG_FLASH_FAIL_BIT_SET:
        return "BSL_MSG_FLASH_FAIL_BIT_SET";
    case BSL_MSG_VOLTAGE_CHANGE:
        return "BSL_MSG_VOLTAGE_CHANGE";
    case BSL_MSG_LOCKED:
        return "BSL_MSG_LOCKED";
    case BSL_MSG_PASSWORD_ERROR:
        return "BSL_MSG_PASSWORD_ERROR";
    case BSL_MSG_BYTE_WRITE_FORBIDDEN:
        return "BSL_MSG_BYTE_WRITE_FORBIDDEN";
    case BSL_MSG_UNKNOWN_COMMAND:
        return "BSL_MSG_UNKNOWN_COMMAND";
    case BSL_MSG_PACKET_TOO_BIG:
        return "BSL_MSG_PACKET_TOO_BIG";
    case BSL_MSG_INVALID_APP:
        return "BSL_MSG_INVALID_APP";
    default:
        return "Unknown message";
    }
}

#define BSL_MSG_PASSWORD_LEN                (32)

#define BSL_READ_TIMEOUT_MS                 (1000)

#define BSL_NUM_OF_MAGIC_BYTES              (4)
#define BSL_MAGIC_BYTES_START               (0x4400)
#define BSL_MAGIC_BYTES_END                 (BSL_MAGIC_BYTES_START + BSL_NUM_OF_MAGIC_BYTES - 1)

#endif // #ifndef MSP430_BSL_PROG_BSL_DEFS_H
