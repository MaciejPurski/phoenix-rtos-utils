/*
 * MSP430 BSL programming library HAL
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "../../include/bsl_defs.h"
#include "../hal.h"

#include <log.h>

static int _serial_fd = -1;

static int _tiocm_set_clear_bits(int fd, int set, int clear);
static int _set_pin_state(int pin, int state, int inverted);

int bsl_hal_init(void)
{
    return BSL_RES_OK;
}

int bsl_hal_serial_open(const char *device)
{
    if (!device || strlen(device) == 0) {
        log_error("Missing or empty device name");
        return BSL_RES_ARG_ERROR;
    }

    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        log_error("Failed to open the device (%d)", fd);
        return BSL_RES_SERIAL_INIT_ERROR;
    }

    struct termios attr;
    if (tcgetattr(fd, &attr) < 0) {
        log_error("tcgetattr failed");
        return BSL_RES_SERIAL_INIT_ERROR;
    }

    memset(&attr, 0, sizeof(attr));
    cfmakeraw(&attr);

    attr.c_iflag = CRTSCTS;
    attr.c_cflag &= ~CSIZE & ~CSTOPB;
    attr.c_cflag |= PARENB | CS8 | CREAD | CLOCAL; /* Enable parity bit generation and checking */

    attr.c_cc[VMIN] = 1;
    attr.c_cc[VTIME] = 0;

    /* Set baudrate (B115200) */
    if(cfsetispeed(&attr, B115200) < 0 || cfsetospeed(&attr, B115200) < 0) {
        log_error("Failed to set baudrate");
        return BSL_RES_SERIAL_INIT_ERROR;
    }

    if (tcflush(fd, TCIOFLUSH) < 0) {
        log_error("tcflush failed");
        return BSL_RES_SERIAL_INIT_ERROR;
    }

    if (tcsetattr(fd, TCSANOW, &attr) < 0) {
        log_error("tcsetattr failed");
        return BSL_RES_SERIAL_INIT_ERROR;
    }

    _serial_fd = fd;

    return BSL_RES_OK;
}

int bsl_hal_serial_write(const uint8_t *data, uint32_t len)
{
    if (_serial_fd < 0) {
        log_error("Serial closed (fd = %d)", _serial_fd);
        return BSL_RES_SERIAL_CLOSED;
    }

    while (len) {

        int res = write(_serial_fd, data, len);
        if (res < 0) {
            log_error("write failed (%d)", res);
            return BSL_RES_SERIAL_IO_ERROR;
        }

        data += res;
        len -= res;
    }

    return BSL_RES_OK;
}

int bsl_hal_serial_read(uint8_t *data, uint32_t len, uint32_t timeout_ms)
{
    if (_serial_fd < 0) {
        log_error("Serial closed (fd = %d)", _serial_fd);
        return BSL_RES_SERIAL_CLOSED;
    }

    int res;
    fd_set rfds;
    struct timeval tv;

    while (len) {

        FD_ZERO(&rfds);
        FD_SET(_serial_fd, &rfds);

        tv.tv_sec = timeout_ms/1000;
        tv.tv_usec = (timeout_ms%1000)*1000;

        res = select(_serial_fd + 1, &rfds, NULL, NULL, &tv);
        if (res < 0) {
            log_error("select failed (%d)", res);
            return BSL_RES_SERIAL_IO_ERROR;
        }
        if (res == 0) {
            log_error("Timeout occurred while trying to read data");
            return BSL_RES_TIMEOUT;
        }

        res = read(_serial_fd, data, len);
        if (res < 0) {
            log_error("read failed (%d)", res);
            return BSL_RES_SERIAL_IO_ERROR;
        }
        if (res == 0) {
            log_error("Serial closed (fd = %d)", _serial_fd);
            return BSL_RES_SERIAL_CLOSED;
        }

        data += res;
        len -= res;
    }

    return BSL_RES_OK;
}

int bsl_hal_serial_close(void)
{
    return BSL_RES_OK;
}

int bsl_hal_set_tst_state(int state, int inverted)
{
    /* RTS line is used as TEST signal */
    return _set_pin_state(TIOCM_RTS, state, inverted);
}

int bsl_hal_set_rst_state(int state, int inverted)
{
    /* DTR line is used as RESET signal */
    return _set_pin_state(TIOCM_DTR, state, inverted);
}

int bsl_hal_sleep_ms(unsigned ms)
{
    if (usleep(1000*ms) != 0) {
        log_error("usleep failed (errno = %d)", errno);
        return BSL_RES_ERROR;
    }

    return BSL_RES_OK;
}

static int _set_pin_state(int pin, int state, int inverted)
{
    if (_serial_fd < 0) {
        log_error("Serial closed (fd = %d)", _serial_fd);
        return BSL_RES_SERIAL_CLOSED;
    }

    if (inverted) state = !state;

    int set = 0, clear = 0;
    if (state != 0) {
        set = pin;
    } else {
        clear = pin;
    }

    return _tiocm_set_clear_bits(_serial_fd, set, clear);
}

static int _tiocm_set_clear_bits(int fd, int set, int clear)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) < 0) {
        log_error("TIOCMGET ioctl failed");
        return BSL_RES_SERIAL_IO_ERROR;
    }

    status |= set;
    status &= ~clear;

    if (ioctl(fd, TIOCMSET, &status) < 0) {
        log_error("TIOCMSET ioctl failed");
        return BSL_RES_SERIAL_IO_ERROR;
    }

    return BSL_RES_OK;
}
