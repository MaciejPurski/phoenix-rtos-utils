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
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "../../include/bsl_defs.h"
#include "../hal.h"

#include <log.h>

static int _serial_fd = -1;
static struct termios original_setting;

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

    memcpy(&original_setting, &attr, sizeof(struct termios));

    attr.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    attr.c_oflag &= ~OPOST;
    attr.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    attr.c_cflag &= ~(CSIZE | CSTOPB);
    attr.c_cflag |= PARENB | CS8 | CREAD | CLOCAL;

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
    if (tcsetattr(_serial_fd, TCSANOW, &original_setting) < 0) {
        log_error("tcsetattr failed");
        return BSL_RES_SERIAL_CLOSE_ERROR;
    }

    if (tcflush(_serial_fd, TCIOFLUSH) < 0) {
        log_error("tcflush failed");
        return BSL_RES_SERIAL_CLOSE_ERROR;
    }

    if (close(_serial_fd) != 0) {
        log_error("close failed (errno=%s)", strerror(errno));
        return BSL_RES_SERIAL_CLOSE_ERROR;
    }

    return BSL_RES_OK;
}

int bsl_hal_set_tst_state(int state, int inverted)
{
    (void)state;
    (void)inverted;

    return BSL_RES_OK;
}

int bsl_hal_set_rst_state(int state, int inverted)
{
    (void)state;
    (void)inverted;

    return BSL_RES_OK;
}

int bsl_hal_sleep_ms(unsigned ms)
{
    if (usleep(1000*ms) != 0) {
        log_error("usleep failed (errno = %d)", errno);
        return BSL_RES_ERROR;
    }

    return BSL_RES_OK;
}
