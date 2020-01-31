/*
 * Library for communication between MSP430 and the host processor
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef MSP_MON_PROT_H
#define MSP_MON_PROT_H

#include <stddef.h>
#include <stdint.h>

#define MMP_MAX_PAYLOAD_LEN                     (128)

#define MMP_DEFAULT_TX_ACK_TIMEOUT              (1024) /* Number of updates */

#define MMP_RES__OK                             (0)
#define MMP_RES__ARG_ERROR                      (-1)
#define MMP_RES__INTERNAL_ERROR                 (-2)
#define MMP_RES__TX_BUSY                        (-3)
#define MMP_RES__PAYLOAD_TOO_LONG               (-4)
#define MMP_RES__ACK_TIMEOUT                    (-5) /* Timeout while waiting for ACK */
#define MMP_RES__FCS_ERROR                      (-7) /* Frame Control Sequence (CRC16-CCITT) error */
#define MMP_RES__INVALID_PACKET                 (-8)
#define MMP_RES__NACK                           (-9)
#define MMP_RES__ACK_NACK_CMD_MISMATCH          (-10) /* ACK/NACK received, but CMDs do not match */
#define MMP_RES__MISSING_RX_HANDLER             (-11)
#define MMP_RES__CONTINUE_UPDATE                (-12)
#define MMP_RES__READ_FAILED                    (-13)
#define MMP_RES__WRITE_FAILED                   (-14)
#define MMP_RES__PACKET_TOO_SHORT               (-15)
#define MMP_RES__RX_HANDLER_FAILED              (-16)
#define MMP_RES__ACK_NACK_SEQ_MISMATCH          (-17) /* ACK/NACK received, but sequence numbers do not match */
#define MMP_RES__TX_CLBK_ERROR                  (-18)
#define MMP_RES__HDLC_ERROR                     (-19)
#define MMP_RES__UNINITIALIZED                  (-20)
#define MMP_RES__DENINITIALIZED                 (-21)
#define MMP_RES__TX_DISABLED                    (-22)
#define MMP_RES__UNSUPPORTED_CMD                (-23)
#define MMP_RES__CMD_EXECUTION_ERROR            (-24)

#define MMP_CMD__GET_VERSION                    (0x00)
#define MMP_CMD__GET_STATUS                     (0x01)
#define MMP_CMD__GET_TIME                       (0x02)
#define MMP_CMD__SET_TIME                       (0x03)
#define MMP_CMD__ENTER_BOOTLOADER               (0x04)
#define MMP_CMD__READ_EVENTS                    (0x05)
#define MMP_CMD__WDG_REFRESH                    (0x06)
#define MMP_CMD__LOG_MSG                        (0x07)
#define MMP_CMD__PUSH_EVENT                     (0x08)
#define MMP_CMD__GET_VBAT                       (0x09)
#define MMP_CMD__GET_VPRI                       (0x0a)
#define MMP_CMD__GET_VSEC                       (0x0b)
#define MMP_CMD__GET_TEMP0                      (0x0c)
#define MMP_CMD__GET_TEMP1                      (0x0d)
#define MMP_CMD__ENABLE_PUSHING_EVENTS          (0x0e)
#define MMP_CMD__DISABLE_PUSHING_EVENTS         (0x0f)
#define MMP_CMD__GET_STATE_FLAGS                (0x10)
#define MMP_CMD__GET_BOOT_REASON                (0x11)

#define MMP_CMD__ACK_FLAG                       (0x80)
#define MMP_CMD__NACK_FLAG                      (0x40)

#define MMP_CMD__IS_ACK_NACK(type)              \
    (type & (MMP_CMD__ACK_FLAG | MMP_CMD__NACK_FLAG))

#define MMP_CMD__CLEAR_ACK_NACK(type)           \
    (type & (~(MMP_CMD__ACK_FLAG | MMP_CMD__NACK_FLAG)))

/* Error codes carried by NACK packets */
#define MMP_NACK__FCS_ERROR                     (0x01)

typedef struct {
    /*  ------- ------- ------- ------- ------- ------- ------- -------
     * | Bit 7 | Bit 6 | Bit 5 | Bit 4 | Bit 3 | Bit 2 | Bit 1 | Bit 0 |
     *  ------- ------- ------- ------- ------- ------- ------- -------
     * | ACK   | NACK  |                   COMMAND                     |
     *  ------- ------- -----------------------------------------------  */
    uint8_t type;

    /* Sequence number */
    uint8_t seq;
} mmp_header_t;

#define MMP_BUFFER_LEN                          \
    (2*MMP_MAX_PAYLOAD_LEN + 6 + 2*sizeof(mmp_header_t)) /* Escaped payload, 2 term bytes, 4 escaped FCS bytes and escaped header */

#define MMP_MIN_PACKET_SIZE                     \
    (4 + sizeof(mmp_header_t)) /* 2 term bytes, 2 FCS bytes and header */

typedef int (*mmp_rx_handler_t)(uint8_t cmd,
                                const uint8_t *data,
                                uint16_t data_len,
                                uint8_t *resp,
                                uint16_t *resp_len);

typedef int (*mmp_tx_done_clbk_t)(int res,
                                  const uint8_t *data,
                                  uint16_t len,
                                  void *arg);

typedef int (*mmp_write_func_t)(const uint8_t *data, uint16_t len);

typedef int (*mmp_read_func_t)(uint8_t *byte);

typedef enum {
    mmp_rx_state__receiving,
    mmp_rx_state__fcs_error,
    mmp_rx_state__packet_pending,
} mmp_rx_state_t;

typedef enum {
    mmp_tx_state__idle,
    mmp_tx_state__sending_data,
    mmp_tx_state__waiting_for_ack,
    mmp_tx_state__sending_ack,
    mmp_tx_state__sa_and_wfa, /* Sending ACK and waiting for ACK at the same time */
} mmp_tx_state_t;

typedef struct {
    mmp_tx_state_t tx_state;
    mmp_rx_state_t rx_state;

    mmp_read_func_t read_byte;
    mmp_write_func_t write;

    mmp_rx_handler_t rx_handler;

    uint8_t rx_buffer[MMP_BUFFER_LEN];
    uint16_t rx_read; /* Number of bytes read into RX buffer */

    mmp_header_t rx_header;
    uint8_t *rx_data;
    uint8_t rx_data_len;

    uint8_t tx_buffer[MMP_BUFFER_LEN];
    uint16_t tx_packet_len;
    uint16_t tx_sent;

    mmp_tx_done_clbk_t tx_done_clbk;
    void *tx_done_clbk_arg;
    mmp_header_t tx_header;
    uint16_t tx_timeout;

    uint8_t tx_seq;

    int initialized;
    int tx_enabled;
} mmp_t;

typedef struct __attribute__((packed)) {
    uint32_t unix_time;
} mmp_time_t;

typedef struct __attribute__((packed)) {
    int16_t error_code;
} mmp_nack_t;

typedef struct __attribute__((packed)) {
    uint32_t voltage; /* mV */
} mmp_voltage_t;

typedef struct __attribute__((packed)) {
    int32_t temp; /* degrees Celsius * 10^(-3) */
} mmp_temperature_t;

typedef struct __attribute__((packed)) {
    uint32_t major;
    uint32_t minor;
    uint32_t patch;
} mmp_version_t;

#define MMP_STATUS__OK                          (0)
#define MMP_STATUS__INIT_ERROR                  (1)
#define MMP_STATUS__DEINIT_ERROR                (2)
#define MMP_STATUS__UPDATE_ERROR                (3)
#define MMP_STATUS__GENERAL_ERROR               (4)
#define MMP_STATUS__FAIL                        (MMP_STATUS__GENERAL_ERROR)

typedef struct __attribute__((packed)) {
    /* Current status/error code for each subsystem */
    int8_t accel;       /* Accelerometer */
    int8_t mag;         /* Magnetometer */
    int8_t fram;        /* External FRAM */
    int8_t event;       /* Event subsystem */
    int8_t log;         /* Log subsystem */
    int8_t tampers;
    int8_t clock32kHz;
    int8_t battery;
    int8_t mainPower;
    int8_t auxPower;

    /* Flag indicating if events will be automatically pushed to the host */
    int8_t sendingEventsEnabled;
} mmp_status_t;

typedef enum {
    mmp_state_flag__tampered_1,
    mmp_state_flag__tampered_2,
    mmp_state_flag__mag_alarm_x,
    mmp_state_flag__mag_alarm_y,
    mmp_state_flag__mag_alarm_z,
    mmp_state_flag__main_power_fail,
    mmp_state_flag__aux_power_fail,
    mmp_state_flag__battery_fail,
} mmp_state_flag_t;

typedef uint32_t mmp_state_flags_t;

#define MMP_HOST_BOOT_REASON__WDG               (0) /* Reset by external watchdog */
#define MMP_HOST_BOOT_REASON__PWR               (1) /* Power up */

typedef int8_t mmp_host_boot_reason_t;

int mmp_init(mmp_t *mmp,
             mmp_read_func_t read_byte,
             mmp_write_func_t write,
             mmp_rx_handler_t rx_handler);

int mmp_update(mmp_t *mmp);

int mmp_transmit(mmp_t *mmp,
                 uint8_t cmd,
                 const uint8_t *data,
                 uint16_t len,
                 mmp_tx_done_clbk_t clbk,
                 void *clbk_arg,
                 uint16_t timeout);

int mmp_is_ready_to_transmit(mmp_t *mmp);

mmp_t* mmp_get_default_instance(void);
void mmp_set_default_instance(mmp_t* mmp);

void mmp_deinit(mmp_t *mmp);

void mmp_enable_tx(mmp_t *mmp);
void mmp_disable_tx(mmp_t *mmp);

#endif /* MSP_MON_PROT_H */
