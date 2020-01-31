/*
 * Phoenix-RTOS
 *
 * MSP430 Monitor Daemon
 *
 * Copyright 2018 Phoenix Systems
 * Author: Krystian Wasik
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <syslog.h>

#include <sys/threads.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/platform.h>

#include <phoenix/arch/imx6ull.h>

#include <msp_mon_prot.h>
#include <ps_dcsap_logger.h>
#include <ps_log.h>

#include "../../app/event/event_defs.h"
#include "../../app/version.h"
#include "rtc.h"

#if 0
#define COLOR_RED     "\033[1;31m"
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_NORMAL  "\033[0m"
#else
#define COLOR_RED
#define COLOR_CYAN
#define COLOR_YELLOW
#define COLOR_NORMAL
#endif

#define LOG_TAG "lemond: "
#define log_debug(fmt, ...)     do { log_printf(LOG_DEBUG, fmt "\n", ##__VA_ARGS__); } while (0)
#define log_info(fmt, ...)      do { log_printf(LOG_INFO, COLOR_CYAN fmt "\n" COLOR_NORMAL, ##__VA_ARGS__); } while (0)
#define log_warn(fmt, ...)      do { log_printf(LOG_WARNING, COLOR_YELLOW fmt "\n" COLOR_NORMAL, ##__VA_ARGS__); } while (0)
#define log_error(fmt, ...)     do { log_printf(LOG_ERR, COLOR_RED  fmt "\n" COLOR_NORMAL, ##__VA_ARGS__); } while (0)

#define SERIAL_DEV_NAME         "/dev/uart4"

#define MMP_UPDATE_RATE         100 /* Hz */
#define MMP_THD_SLEEP_US        (1000*1000/MMP_UPDATE_RATE)
#define MMP_SEND_TIMEOUT_US     (500*1000)

#define RTC_DEV_ID              (0)
#define VBAT_DEV_ID             (1)
#define VPRI_DEV_ID             (2)
#define VSEC_DEV_ID             (3)
#define TEMP0_DEV_ID            (4)
#define TEMP1_DEV_ID            (5)
#define ACCEL_DEV_ID            (6)
#define MAG_DEV_ID              (7)
#define TAMPER_0_DEV_ID         (8)
#define TAMPER_1_DEV_ID         (9)
#define BOOT_REASON_DEV_ID      (10)

#define DEV_DIR                 "/dev"

#define VBAT_DEVICE_FILE_NAME           "vbat"
#define VPRI_DEVICE_FILE_NAME           "vpri"
#define VSEC_DEVICE_FILE_NAME           "vsec"
#define TEMP0_DEVICE_FILE_NAME          "temp0"
#define TEMP1_DEVICE_FILE_NAME          "temp1"
#define ACCEL_DEVICE_FILE_NAME          "accel_alarm"
#define MAG_DEVICE_FILE_NAME            "mag_alarm"
#define TAMPER_0_DEVICE_FILE_NAME       "cable_cover"
#define TAMPER_1_DEVICE_FILE_NAME       "main_cover"
#define BOOT_REASON_DEVICE_FILE_NAME    "bootreason"

struct {
    uint32_t port;

    int exit;

    int serial_fd;

    mmp_t mmp;
    handle_t mmp_lock;
    handle_t mmp_tx_idle;

    int display_msp_logs;
    int log_level;
    int syslog;
    int dcsap_available;

    mmp_status_t prev_status;
    int prev_status_valid;
    int msp_broken;

    /* Thread priorities */
    int mmp_thd_prio;
    int worker_prio;
    int main_prio;

    /* Time of the last orientation change */
    time_t last_accel_alarm;

    /* Defines for how long after receiving the event notifying about
     * orientation change should the alarm flag be kept high.
     * Expressed in seconds. */
    time_t keep_accel_alarm_for;

    /* Firmware version at the time of initialization */
    mmp_version_t initial_version;
    int initial_version_unknown; /* Indicates if firmware version was successfully read */

    time_t updated_at;
    int update_event_pending;

    int boot_reason;
} common;

#define BOOT_REASON__UNKNOWN        (-1)

#define BOOT_REASON__EXTERNAL_WDG   (MMP_HOST_BOOT_REASON__WDG)
#define BOOT_REASON__POWER_ON       (MMP_HOST_BOOT_REASON__PWR)

#define BOOT_REASON__SOFT           (20)
#define BOOT_REASON__INTERNAL_WDG   (21)
#define BOOT_REASON__JTAG           (22)
#define BOOT_REASON__CSU            (23)
#define BOOT_REASON__ONOFF          (24)
#define BOOT_REASON__TEMP_SENS      (25)

static void log_printf(int lvl, const char* fmt, ...)
{
    va_list arg;

    if (lvl > common.log_level)
        return;

    va_start(arg, fmt);
    if (!common.syslog) {
        printf("%s", LOG_TAG);
        vprintf(fmt, arg);
    } else {
        vsyslog(lvl, fmt, arg);
    }
    va_end(arg);
}

#define DATE_BUF_SIZE   (64)

#define DCSAP_REASON_TAMPER             "tamper"
#define DCSAP_REASON_POWERFAIL          "powerfail"
#define DCSAP_REASON_EXT_CTLR_RESTART   "ext_ctlr_restart"
#define DCSAP_REASON_EXT_CTLR_UPDATE    "ext_ctlr_update"

#define DCSAP_TAMPER1_NAME      "cable"
#define DCSAP_TAMPER2_NAME      "main"
#define DCSAP_TAMPER_ACCEL_NAME "accel"
#define DCSAP_TAMPER_MAGN_NAME  "magn"

#define DCSAP_AXIS_X            "X"
#define DCSAP_AXIS_Y            "Y"
#define DCSAP_AXIS_Z            "Z"

#define DCSAP_TAMPER_OPENED     0
#define DCSAP_TAMPER_CLOSED     1

#define DCSAP_POWERFAIL_BEGIN   1
#define DCSAP_POWERFAIL_END     0

#define DCSAP_POWERFAIL_DEVNAME_MAIN "main"
#define DCSAP_POWERFAIL_DEVNAME_AUX  "auxiliary"
#define DCSAP_POWERFAIL_DEVNAME_BAT  "battery"

static int send_event(const char* reason,
                       uint32_t unix_time,
                       int8_t status,
                       const char* source_name,
                       const char* comment)
{
    int res;
    char date_buf[DATE_BUF_SIZE];
    struct tm t;
    time_t time = unix_time;

    gmtime_r(&time, &t);
    strftime(date_buf, DATE_BUF_SIZE, "%a %b %e %H:%M:%S %Y", &t);

    log_info("new event: [reason=%s, status=%d, arg1=%s, arg2=%s] at %s",
        reason, status, source_name, comment, date_buf);

    res = ps_dcsap_logger_send_event(reason, unix_time, status, source_name, comment);
    if (res != 0) {
        common.dcsap_available = 0;
        log_error("DCSAP server is unavailable");
        return -1;
    }

    return 0;
}



static int handle_event(event_t *event)
{
    time_t time = event->timestamp;

    char date_buf[DATE_BUF_SIZE];
    struct tm t;

    gmtime_r(&time, &t);
    strftime(date_buf, DATE_BUF_SIZE, "%a %b %e %H:%M:%S %Y", &t);

    switch (event->type) {
    case EVENT_IMX_WDG_RESET:
        log_warn("IMX was reset by WDG at %s", date_buf);
        break;
    case EVENT_TAMPER_1_START:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_OPENED, DCSAP_TAMPER1_NAME, NULL);
    case EVENT_TAMPER_1_STOP:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_CLOSED, DCSAP_TAMPER1_NAME, NULL);
    case EVENT_TAMPER_2_START:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_OPENED, DCSAP_TAMPER2_NAME, NULL);
    case EVENT_TAMPER_2_STOP:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_CLOSED, DCSAP_TAMPER2_NAME, NULL);

    case EVENT_MAG_X_START:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_OPENED, DCSAP_TAMPER_MAGN_NAME, DCSAP_AXIS_X);
    case EVENT_MAG_X_STOP:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_CLOSED, DCSAP_TAMPER_MAGN_NAME, DCSAP_AXIS_X);
    case EVENT_MAG_Y_START:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_OPENED, DCSAP_TAMPER_MAGN_NAME, DCSAP_AXIS_Y);
    case EVENT_MAG_Y_STOP:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_CLOSED, DCSAP_TAMPER_MAGN_NAME, DCSAP_AXIS_Y);
    case EVENT_MAG_Z_START:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_OPENED, DCSAP_TAMPER_MAGN_NAME, DCSAP_AXIS_Z);
    case EVENT_MAG_Z_STOP:
        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_CLOSED, DCSAP_TAMPER_MAGN_NAME, DCSAP_AXIS_Z);

    case EVENT_ACCEL_ORIENTATION:
        /* If event notifies about more recent orientation change */
        if (time > common.last_accel_alarm)
            common.last_accel_alarm = time;

        return send_event(DCSAP_REASON_TAMPER, time, DCSAP_TAMPER_OPENED, DCSAP_TAMPER_ACCEL_NAME, NULL);

    case EVENT_MAIN_POWER_OUTAGE:
        return send_event(DCSAP_REASON_POWERFAIL, time, DCSAP_POWERFAIL_BEGIN, DCSAP_POWERFAIL_DEVNAME_MAIN, NULL);
    case EVENT_MAIN_POWER_BACK:
        return send_event(DCSAP_REASON_POWERFAIL, time, DCSAP_POWERFAIL_END, DCSAP_POWERFAIL_DEVNAME_MAIN, NULL);
    case EVENT_AUX_POWER_OUTAGE:
        return send_event(DCSAP_REASON_POWERFAIL, time, DCSAP_POWERFAIL_BEGIN, DCSAP_POWERFAIL_DEVNAME_AUX, NULL);
    case EVENT_AUX_POWER_BACK:
        return send_event(DCSAP_REASON_POWERFAIL, time, DCSAP_POWERFAIL_END, DCSAP_POWERFAIL_DEVNAME_AUX, NULL);
    case EVENT_BATTERY_LOW:
        return send_event(DCSAP_REASON_POWERFAIL, time, DCSAP_POWERFAIL_BEGIN, DCSAP_POWERFAIL_DEVNAME_BAT, NULL);
    case EVENT_BATTERY_OK:
        return send_event(DCSAP_REASON_POWERFAIL, time, DCSAP_POWERFAIL_END, DCSAP_POWERFAIL_DEVNAME_BAT, NULL);

    case EVENT_MSP_RESET:
        log_warn("RTC was reset at %s. RTC time may be invalid.", date_buf);
        return send_event(DCSAP_REASON_EXT_CTLR_RESTART, time, 0, "", NULL);

    default:
        log_error("received event of unrecognized type (%u)", event->type);
        break;
    }

    return 0;
}

static int mmp_read(uint8_t *byte)
{
    int res = read(common.serial_fd, byte, sizeof(uint8_t));

    if (res < 0) {
        if (errno == EWOULDBLOCK)
            return 0;
        log_error("mmp_read: read failed (res=%d, errno=%d [%s])", res, errno, strerror(errno));
    }

    return res;
}

static int mmp_write(const uint8_t *data, uint16_t len)
{
    int res = write(common.serial_fd, data, len);

    if (res < 0) {
        if (errno == EWOULDBLOCK)
            return 0;
        log_error("mmp_read: write failed (res=%d, errno=%d [%s])", res, errno, strerror(errno));
    }

    return res;
}

static int mmp_rx_handler(uint8_t cmd,
                          const uint8_t *data,
                          uint16_t data_len,
                          uint8_t *resp,
                          uint16_t *resp_len)
{
    (void)resp;

    *resp_len = 0;

    switch (cmd) {
    case MMP_CMD__LOG_MSG:
        if (common.display_msp_logs)
            log_info("log_msg: %s", (char*)data);
        break;
    case MMP_CMD__PUSH_EVENT:
        if (data_len != sizeof(event_t)) {
            log_error("got MMP_CMD__PUSH_EVEMTS with invalid size (%u)", data_len);
            return MMP_RES__INVALID_PACKET;
        }
        event_t event;
        memcpy(&event, data, sizeof(event_t));
        if (handle_event(&event) != 0)
            return MMP_RES__NACK;
        break;
    default:
        log_error("received unsupported MMP command (%u)", cmd);
        break;
    }

    return MMP_RES__OK;
}

typedef struct {
    handle_t cond;
    int res;
    uint8_t *data;
    uint16_t len;
} mmp_tx_done_clbk_params_t;

static int mmp_tx_done(int res, const uint8_t *data, uint16_t len, void *arg)
{
    mmp_tx_done_clbk_params_t *params = (mmp_tx_done_clbk_params_t*)arg;

    if (res == MMP_RES__OK) {
        if (params->len >= len) {
            memcpy(params->data, data, len);
        } else {
            log_error("mmp_tx_done: buffer for response is too small (%u vs %u)", params->len, len);
            res = MMP_RES__PAYLOAD_TOO_LONG;
        }

    } else if (res == MMP_RES__ACK_TIMEOUT) {
        // log_error("mmp_tx_done: ack timeout");

    } else if (res == MMP_RES__NACK && len == sizeof(mmp_nack_t)) {
        mmp_nack_t nack;
        memcpy(&nack, data, sizeof(mmp_nack_t));
        log_error("mmp_tx_done: nack received (error_code = %d)", nack.error_code);

    } else {
        log_error("mmp_tx_done: ack/nack error (res=%d, len=%u)", res, len);
    }

    params->res = res;
    params->len = len;

    condSignal(params->cond);

    return 0;
}

static int mmp_send(mmp_t *mmp,
                    uint8_t cmd,
                    const uint8_t *in,
                    uint16_t in_len,
                    uint8_t *out,
                    uint16_t out_len)
{
    int res, tries = 3;
    mmp_tx_done_clbk_params_t clbk_params;

    res = condCreate(&clbk_params.cond);
    if (res != EOK) {
        log_error("failed to create tx done cond (%d)", res);
        return -1;
    }

    while (1) {

        mutexLock(common.mmp_lock);
        condWait(common.mmp_tx_idle, common.mmp_lock, 0);

        clbk_params.data = out;
        clbk_params.len = out_len;

        uint16_t timeout = MMP_SEND_TIMEOUT_US/MMP_THD_SLEEP_US + 1; /* Expressed in number of updates */
        res = mmp_transmit(mmp, cmd, in, in_len, mmp_tx_done, &clbk_params, timeout);
        if (res != MMP_RES__OK) {
            log_error("mmp_transmit failed (%d)", res);
            mutexUnlock(common.mmp_lock);
            continue;
        }

        condWait(clbk_params.cond, common.mmp_lock, 0);
        mutexUnlock(common.mmp_lock);

        res = clbk_params.res;
        if (res == MMP_RES__OK)
            break;

        if (--tries == 0) {
            goto fail;
        }
    }

    /* Verify size of the response data */
    if (clbk_params.len != out_len) {
        res = MMP_RES__INVALID_PACKET;
        goto fail;
    }

fail:

    resourceDestroy(clbk_params.cond);
    return res;
}

int serial_init(const char *device_name)
{
    unsigned tries = 100;
    while ((common.serial_fd = open(device_name, O_RDWR | O_NONBLOCK)) < 0) {
        usleep(100000);
        if (--tries == 0)
            break;
    }

    if (common.serial_fd < 0) {
        log_error("failed to open %s", device_name);
        return -1;
    }

    return 0;
}

int serial_close(void)
{
    return close(common.serial_fd);
}

static int rtc_get_time(struct rtc_time *rtc_time)
{
    int res;
    mmp_time_t mmp_time;

    if (rtc_time == NULL)
        return -EINVAL;

    res = mmp_send(&common.mmp, MMP_CMD__GET_TIME, NULL, 0, (uint8_t*)&mmp_time, sizeof(mmp_time_t));
    if (res < 0)
        return -EIO;

    time_t time = mmp_time.unix_time;

    struct tm t;
    gmtime_r(&time, &t);
    rtc_time->tm_sec    = t.tm_sec;
    rtc_time->tm_min    = t.tm_min;
    rtc_time->tm_hour   = t.tm_hour;
    rtc_time->tm_mday   = t.tm_mday;
    rtc_time->tm_mon    = t.tm_mon;
    rtc_time->tm_year   = t.tm_year;
    rtc_time->tm_wday   = t.tm_wday;
    rtc_time->tm_yday   = t.tm_yday;
    rtc_time->tm_isdst  = t.tm_isdst;

    return EOK;
}

static int rtc_set_time(const struct rtc_time *rtc_time)
{
    int res;
    mmp_time_t mmp_time;

    if (rtc_time == NULL)
        return -EINVAL;

    struct tm t;
    t.tm_sec    = rtc_time->tm_sec;
    t.tm_min    = rtc_time->tm_min;
    t.tm_hour   = rtc_time->tm_hour;
    t.tm_mday   = rtc_time->tm_mday;
    t.tm_mon    = rtc_time->tm_mon;
    t.tm_year   = rtc_time->tm_year;
    t.tm_wday   = rtc_time->tm_wday;
    t.tm_yday   = rtc_time->tm_yday;
    t.tm_isdst  = rtc_time->tm_isdst;

    time_t time = mktime(&t);

    mmp_time.unix_time = time;
    res = mmp_send(&common.mmp, MMP_CMD__SET_TIME, (uint8_t*)&mmp_time, sizeof(mmp_time_t), NULL, 0);
    if (res < 0)
        return -EIO;

    return EOK;
}

static int dev_init(const char *dir, const char *name, id_t id)
{
    int res;
    oid_t dir_oid;
    msg_t msg;

    res = mkdir(dir, 0);
    if (res < 0 && errno != EEXIST) {
        log_error("mkdir %s failed (errno=%s)", dir, strerror(errno));
        return -1;
    }

    if ((res = lookup(dir, NULL, &dir_oid)) < 0) {
        log_error("%s lookup failed (%d)", dir, res);
        return -1;
    }

    msg.type = mtCreate;
    msg.i.create.type = otDev;
    msg.i.create.mode = 0;
    msg.i.create.dev.port = common.port;
    msg.i.create.dev.id = id;
    msg.i.create.dir = dir_oid;
    msg.i.data = (void*)name;
    msg.i.size = strlen(name) + 1;
    msg.o.data = NULL;
    msg.o.size = 0;

    if ((res = msgSend(dir_oid.port, &msg)) < 0 || msg.o.create.err != EOK) {
        log_error("could not create %s/%s (res=%d, err=%d)", dir, name, res, msg.o.create.err);
        return -1;
    }

    // log_info("%s/%s initialized", dir, name);

    return 0;
}

static int get_voltage(id_t dev_id, int32_t *val)
{
    int res;

    uint8_t cmd;
    switch (dev_id) {
    case VBAT_DEV_ID:
        cmd = MMP_CMD__GET_VBAT;
        break;
    case VPRI_DEV_ID:
        cmd = MMP_CMD__GET_VPRI;
        break;
    case VSEC_DEV_ID:
        cmd = MMP_CMD__GET_VSEC;
        break;
    default:
        return -ENOSYS;
    }

    mmp_voltage_t mmp_voltage;
    res = mmp_send(&common.mmp, cmd, NULL, 0, (uint8_t*)&mmp_voltage, sizeof(mmp_voltage_t));
    if (res < 0) {
        log_error("get_voltage: mmp_send failed (%d)", res);
        return -EIO;
    }

    *val = mmp_voltage.voltage;

    return EOK;
}

static int get_temperature(id_t dev_id, int32_t *val)
{
    int res;

    uint8_t cmd;
    switch (dev_id) {
    case TEMP0_DEV_ID:
        cmd = MMP_CMD__GET_TEMP0;
        break;
    case TEMP1_DEV_ID:
        cmd = MMP_CMD__GET_TEMP1;
        break;
    default:
        return -ENOSYS;
    }

    mmp_temperature_t mmp_temperature;
    res = mmp_send(&common.mmp, cmd, NULL, 0, (uint8_t*)&mmp_temperature, sizeof(mmp_temperature_t));
    if (res < 0) {
        log_error("get_temperature: mmp_send failed (%d)", res);
        return -EIO;
    }

    *val = mmp_temperature.temp;

    return EOK;
}

static int get_tamper_state(id_t dev_id, int32_t *val)
{
    int res;

    *val = 0;

    if (dev_id == ACCEL_DEV_ID) {

        /* Get current time */
        struct timeval tv;
        if ((res = gettimeofday(&tv, NULL)) < 0) {
            log_debug("gettimeofday failed (%s)", strerror(errno));
            return -EIO;
        }
        time_t now = tv.tv_sec;

        /* Check if orientation chage was recent enough */
        if (common.last_accel_alarm != 0 && (common.last_accel_alarm + common.keep_accel_alarm_for) > now)
            *val = 1;

    } else {

        /* Get state flags */
        mmp_state_flags_t state_flags;
        res = mmp_send(&common.mmp, MMP_CMD__GET_STATE_FLAGS, NULL, 0, (uint8_t*)&state_flags, sizeof(mmp_state_flags_t));
        if (res < 0) {
            log_error("get_tamper_state: mmp_send failed (%d)", res);
            return -EIO;
        }

        mmp_state_flags_t mask;
        switch (dev_id) {
        case MAG_DEV_ID:
            mask = (1 << mmp_state_flag__mag_alarm_x) | (1 << mmp_state_flag__mag_alarm_y) | (1 << mmp_state_flag__mag_alarm_z);
            break;
        case TAMPER_0_DEV_ID:
            mask = 1 << mmp_state_flag__tampered_1;
            break;
        case TAMPER_1_DEV_ID:
            mask = 1 << mmp_state_flag__tampered_2;
            break;
        default:
            return -ENOSYS;
        }

        *val = !!(state_flags & mask);
    }

    return EOK;
}

static int get_sensor_value(id_t dev_id, int32_t *val)
{
    switch (dev_id) {
    case TEMP0_DEV_ID:
    case TEMP1_DEV_ID:
        return get_temperature(dev_id, val);
    case VBAT_DEV_ID:
    case VPRI_DEV_ID:
    case VSEC_DEV_ID:
        return get_voltage(dev_id, val);
    case ACCEL_DEV_ID:
    case MAG_DEV_ID:
    case TAMPER_0_DEV_ID:
    case TAMPER_1_DEV_ID:
        return get_tamper_state(dev_id, val);
    default:
        return -ENOSYS;
    }
}

static int dev_open(oid_t *oid, int flags)
{
    (void)oid;
    (void)flags;

    if (common.msp_broken)
        return -ENXIO;

    return EOK;
}

static int dev_close(oid_t *oid, int flags)
{
    (void)oid;
    (void)flags;

    return EOK;
}

static const char* get_boot_reason_as_string(void)
{
    switch (common.boot_reason) {
    case BOOT_REASON__UNKNOWN:
        return "UNKNOWN";
    case BOOT_REASON__EXTERNAL_WDG:
        return "EXTERNAL_WDG";
    case BOOT_REASON__POWER_ON:
        return "POWER_ON";
    case BOOT_REASON__SOFT:
        return "SOFTWARE_RESET";
    case BOOT_REASON__INTERNAL_WDG:
        return "INTERNAL_WDG";
    case BOOT_REASON__JTAG:
        return "JTAG";
    case BOOT_REASON__CSU:
        return "CSU";
    case BOOT_REASON__ONOFF:
        return "ONOFF";
    case BOOT_REASON__TEMP_SENS:
        return "TEMP_SENS";
    default:
        return "UNRECOGNIZED";
    }
}

static int dev_read(oid_t *oid, offs_t offs, size_t len, void *data)
{
    char buf[64];
    int32_t value;

    if (common.msp_broken)
        return -EIO;

    if (oid->id != BOOT_REASON_DEV_ID) {
        int res = get_sensor_value(oid->id, &value);
        if (res == EOK) {
            snprintf(buf, sizeof(buf), "%d\n", value);
            buf[sizeof(buf) - 1] = 0;
        } else if (res == -EIO) {
            buf[0] = 0;
        } else {
            return res;
        }
    } else {
        snprintf(buf, sizeof(buf), "%s\n", get_boot_reason_as_string());
    }

    ssize_t available = (ssize_t)strlen(buf) - offs;
    if (available <= 0)
        return 0;
    if (available < (ssize_t)len)
        len = available;

    memcpy(data, &buf[offs], len);

    return len;
}

static void dev_ctl(msg_t *msg)
{
    unsigned long request;
    int res;
    id_t id;
    const void *data = ioctl_unpack(msg, &request, &id);
    struct rtc_time time;

    if (id != RTC_DEV_ID) {
        log_error("this device does not support ioctls");
        ioctl_setResponse(msg, request, -ENOSYS, NULL);
        return;
    }

    if (common.msp_broken) {
        ioctl_setResponse(msg, request, -EIO, NULL);
        return;
    }

    switch (request) {
    case RTC_RD_TIME:
        res = rtc_get_time(&time);
        ioctl_setResponse(msg, request, res, &time);
        break;

    case RTC_SET_TIME:
        memcpy(&time, data, sizeof(struct rtc_time));
        res = rtc_set_time(&time);
        ioctl_setResponse(msg, request, res, NULL);
        break;

    default:
        log_error("unsupported ioctl (cmd=0x%x, group=0x%x, type=0x%x, len=%u)",
            request & 0xff, IOCGROUP(request), request & IOC_DIRMASK, IOCPARM_LEN(request));
        ioctl_setResponse(msg, request, -EINVAL, NULL);
        break;
    }
}

static int create_flag_file(const char *path)
{
    int res;

    res = open(path, O_WRONLY | O_CREAT | O_TRUNC);
    if (res < 0) {
        log_error("create_flag_file: open failed (res=%d, errno=%s)", res, strerror(errno));
        return -1;
    }

    return close(res);
}

static int destroy_flag_file(const char *path)
{
    int res;

    res = unlink(path);
    if (res < 0) {
        log_error("destroy_flag_file: unlink failed (res=%d, errno=%s)", res, strerror(errno));
        return -1;
    }

    return 0;
}

#define ACCEL_BROKEN_FILE   "/var/run/accelerometer_broken"
#define MAG_BROKEN_FILE     "/var/run/magnetometer_broken"
#define FRAM_BROKEN_FILE    "/var/run/fram_broken"
#define MSP_BROKEN_FILE     "/var/run/msp_broken"
#define CLOCK_FAULT_FILE    "/var/run/clock32kHz_fault"
#define MSP_FW_UPDATE_FILE  "/var/run/msp_fw_update"

static int update_status(mmp_status_t status)
{
    int prev_valid = common.prev_status_valid;

    if (status.accel != MMP_STATUS__OK && (!prev_valid || status.accel != common.prev_status.accel)) {
        log_error("accelerometer is broken (%d), status.accel");
        create_flag_file(ACCEL_BROKEN_FILE);
    }
    if (status.accel == MMP_STATUS__OK && (prev_valid && status.accel != common.prev_status.accel))
        destroy_flag_file(ACCEL_BROKEN_FILE);

    if (status.mag != MMP_STATUS__OK && (!prev_valid || status.mag != common.prev_status.mag)) {
        log_error("magnetometer is broken (%d)", status.mag);
        create_flag_file(MAG_BROKEN_FILE);
    }
    if (status.mag == MMP_STATUS__OK && (prev_valid && status.mag != common.prev_status.mag))
        destroy_flag_file(MAG_BROKEN_FILE);

    if (status.fram != MMP_STATUS__OK && (!prev_valid || status.fram != common.prev_status.fram)) {
        log_error("FRAM is broken (%d)", status.fram);
        create_flag_file(FRAM_BROKEN_FILE);
    }
    if (status.fram == MMP_STATUS__OK && (prev_valid && status.fram != common.prev_status.fram))
        destroy_flag_file(FRAM_BROKEN_FILE);

    if (status.clock32kHz != MMP_STATUS__OK && (!prev_valid || status.clock32kHz != common.prev_status.clock32kHz)) {
        log_error("32kHz clock fault detected (%d)", status.clock32kHz);
        create_flag_file(CLOCK_FAULT_FILE);
    }
    if (status.clock32kHz == MMP_STATUS__OK && (prev_valid && status.clock32kHz != common.prev_status.clock32kHz)) {
        log_info("32kHz clock stable");
        destroy_flag_file(CLOCK_FAULT_FILE);
    }

    if (status.event != MMP_STATUS__OK && (!prev_valid || status.event != common.prev_status.event))
        log_error("event subsystem is broken (%d)", status.event);
    if (status.log != MMP_STATUS__OK && (!prev_valid || status.log != common.prev_status.log))
        log_error("log subsystem is broken (%d)", status.log);
    if (status.tampers != MMP_STATUS__OK && (!prev_valid || status.tampers != common.prev_status.tampers))
        log_error("tamper subsystem is broken (%d)", status.tampers);

    common.prev_status = status;
    common.prev_status_valid = 1;

    return 0;
}

static void mmp_thread(void *arg)
{
    int res;

    (void)arg;

    while (1) {

        mutexLock(common.mmp_lock);

        res = mmp_update(&common.mmp);
        if (res != MMP_RES__OK)
            log_error("mmp update failed (%d)", res);

        if (mmp_is_ready_to_transmit(&common.mmp))
            condSignal(common.mmp_tx_idle);

        mutexUnlock(common.mmp_lock);

        if (!common.exit)
            usleep(MMP_THD_SLEEP_US);
    }
}

static void worker_thread(void *arg)
{
    msg_t msg;
    unsigned rid;

    (void)arg;

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
                msg.o.io.err = dev_read(&msg.i.io.oid, msg.i.io.offs, msg.o.size, msg.o.data);
                break;

            case mtWrite:
                msg.o.io.err = -ENOSYS;
                break;

            case mtDevCtl:
                dev_ctl(&msg);
                break;
        }

        msgRespond(common.port, &msg, rid);
    }
}

/* Expected firmware version */
static const mmp_version_t expected_ver = {
    .major  = MSP_FW_VERSION_MAJOR,
    .minor  = MSP_FW_VERSION_MINOR,
    .patch  = MSP_FW_VERSION_PATCH,
};

static int get_firmware_version(mmp_version_t *current)
{
    int res;

    res = mmp_send(&common.mmp, MMP_CMD__GET_VERSION, NULL, 0, (uint8_t*)current, sizeof(mmp_version_t));
    if (res < 0) {
        log_debug("failed to get firmware version (%d)", res);
        return 1;
    }

    return 0;
}

static int is_firmware_up_to_date(const mmp_version_t *ver)
{
    if ((expected_ver.major != ver->major) || (expected_ver.minor != ver->minor) || (expected_ver.patch != ver->patch)) {
        log_debug("invalid firmware version (expected: %u.%u.%u, current: %u.%u.%u)",
            expected_ver.major, expected_ver.minor, expected_ver.patch,
            ver->major, ver->minor, ver->patch);
        return 0;
    }

    // log_info("firmware version: %u.%u.%u", ver->major, ver->minor, ver->patch);

    return 1;
}

static int is_firmware_upgrade_allowed(void)
{
    const char *path = "/var/preinit";
    FILE *fp;

    unsigned tries = 100;
    while ((fp = fopen(path, "r")) == NULL) {
        usleep(100000);
        if (--tries == 0)
            break;
    }

    if (fp == NULL) {
        log_error("Failed to open %s", path);
        return 0;
    }

    char c;
    if (fread(&c, sizeof(char), 1, fp) != 1) {
        log_error("Failed to read %s", path);
        fclose(fp);
        return 0;
    }

    fclose(fp);

    if (c == 'p') {
        return 1; /* Allow firmware upgrade only on primary kernel */
    } else {
        return 0;
    }
}

static int add_firmware_update_event(void)
{
    char previous_ver[16];
    char new_ver[16];

    if (common.initial_version_unknown) {
        snprintf(previous_ver, sizeof(previous_ver), "unknown");
    } else {
        snprintf(previous_ver, sizeof(previous_ver), "%u.%u.%u",
            common.initial_version.major, common.initial_version.minor, common.initial_version.patch);
    }

     snprintf(new_ver, sizeof(new_ver), "%u.%u.%u",
        expected_ver.major, expected_ver.minor, expected_ver.patch);

    return send_event(DCSAP_REASON_EXT_CTLR_UPDATE, common.updated_at, 0, previous_ver, new_ver);
}

#define FIRMWARE_PATH           "/etc/msp-mon-app.hex"
#define MSP_MON_PROG_PATH       "/sbin/msp-mon-prog"

static int firmware_update(void)
{
    int res, status;

    create_flag_file(MSP_FW_UPDATE_FILE);

    /* Try to enter bootloader. May fail if MSP was not programmed yet */
    mmp_send(&common.mmp, MMP_CMD__ENTER_BOOTLOADER, NULL, 0, NULL, 0);

    /* Make sure no one tries to use mmp during update */
    mutexLock(common.mmp_lock);

    serial_close();

    /* Fork and run firmware update */
    pid_t child_pid = fork();
    if (child_pid == 0) {
        execlp(MSP_MON_PROG_PATH, "-s", "-S", "-d", SERIAL_DEV_NAME, FIRMWARE_PATH, NULL);

        log_error("exec failed");
        exit(EXIT_FAILURE);
    }

    /* TODO: How to handle ECHILD after EINTR? */
    while (1) {
        res = waitpid(child_pid, &status, 0);
        if (res >= 0) {
            break;
        } else if (errno != EINTR) {
            log_error("waitpid failed (res=%d, errno=%d (%s))",res, errno, strerror(errno));
            return -1;
        }
    }

    status = WEXITSTATUS(status);

    if (status != 0) {
        log_error("msp-mon-prog failed (%d)", status);
        return -1;
    }

    res = serial_init(SERIAL_DEV_NAME);
    if (res < 0) {
        log_error("failed to initialize serial after firmware update (%d)", res);
        return -1;
    }

    mutexUnlock(common.mmp_lock);

    destroy_flag_file(MSP_FW_UPDATE_FILE);

    /* Store info about update in order to add event after we have DCSAP communication */
    common.updated_at = time(NULL);
    common.update_event_pending = 1;

    return 0;
}

static int rtc_to_sys_clock(void)
{
    int res;
    mmp_time_t mmp_time;

    /* TODO: Make it faster. Shorter timeout/no multiple tries */
    res = mmp_send(&common.mmp, MMP_CMD__GET_TIME, NULL, 0, (uint8_t*)&mmp_time, sizeof(mmp_time_t));
    if (res < 0) {
        log_debug("failed to get RTC time (%d)", res);
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = mmp_time.unix_time;
    tv.tv_usec = 0;

    res = settimeofday(&tv, NULL);
    if (res < 0) {
        log_debug("settimeofday failed (%s)", strerror(errno));
        return -1;
    }

    return 0;
}

static int sys_clock_to_rtc(void)
{
    int res;
    struct timeval tv;

    res = gettimeofday(&tv, NULL);
    if (res < 0) {
        log_debug("gettimeofday failed (%s)", strerror(errno));
        return -1;
    }

    mmp_time_t mmp_time;
    mmp_time.unix_time = tv.tv_sec;

    res = mmp_send(&common.mmp, MMP_CMD__SET_TIME, (uint8_t*)&mmp_time, sizeof(mmp_time_t), NULL, 0);
    if (res < 0) {
        log_debug("failed to set RTC time (%d)", res);
        return -1;
    }

    return 0;
}

static int get_imx_reset_cause(uint32_t *out)
{
    platformctl_t pctl;

    pctl.type = pctl_reboot;
    pctl.action = pctl_get;

    if (platformctl(&pctl) != EOK)
        return 1;

    *out = pctl.reboot.reason;

    return 0;
}

static int get_boot_reason(void)
{
    int res;

    uint32_t reset_casue;

    if ((res = get_imx_reset_cause(&reset_casue)) != 0) {
        log_error("failed to get IMX reset cause");
        return BOOT_REASON__UNKNOWN;
    }

    switch (reset_casue) {
    case 0x00004: /* CSU */
        return BOOT_REASON__CSU;
    case 0x00008: /* IPP USER */
        return BOOT_REASON__ONOFF;
    case 0x00010: /* WDOG */
    case 0x00080: /* WDOG3 */
        return BOOT_REASON__INTERNAL_WDG;
    case 0x00020: /* JTAG HIGH-Z */
    case 0x00040: /* JTAG SW */
        return BOOT_REASON__JTAG;
    case 0x00100: /* TEMPSENSE */
        return BOOT_REASON__TEMP_SENS;
    case 0x10000: /* WARM BOOT */
        return BOOT_REASON__SOFT;
    case 0x00001: /* POR */
    case 0x00011: /* POR */
        break; /* We'll have to check with MSP */
    default:
        log_error("unrecognized SRC_SRSR value (%u)", reset_casue);
        return BOOT_REASON__UNKNOWN;
    }

    /* Get boot reason from MSP */
    mmp_host_boot_reason_t boot_reason;
    res = mmp_send(&common.mmp, MMP_CMD__GET_BOOT_REASON, NULL, 0, (uint8_t*)&boot_reason, sizeof(boot_reason));
    if (res < 0) {
        log_debug("failed to get boot reason (%d)", res);
        return BOOT_REASON__UNKNOWN;
    }

    return boot_reason;
}

#define MMP_THD_PRIO            (6)
#define MMP_THD_STACK           (4096)
static char mmp_stack[MMP_THD_STACK] __attribute__ ((aligned(8)));

#define WORKER_THD_PRIO         (6)
#define WORKER_THD_STACK        (4096)
static char worker_stack[WORKER_THD_STACK] __attribute__ ((aligned(8)));

#define MAIN_THD_PRIO           (6)

typedef struct {
    const char *dir;
    const char *file_name;
    id_t dev_id;
} device_t;

static device_t devices[] = {
    {RTC_DEVICE_DIR,    RTC_DEVICE_FILE_NAME,           RTC_DEV_ID},
    {DEV_DIR,           VBAT_DEVICE_FILE_NAME,          VBAT_DEV_ID},
    {DEV_DIR,           VPRI_DEVICE_FILE_NAME,          VPRI_DEV_ID},
    {DEV_DIR,           VSEC_DEVICE_FILE_NAME,          VSEC_DEV_ID},
    {DEV_DIR,           TEMP0_DEVICE_FILE_NAME,         TEMP0_DEV_ID},
    {DEV_DIR,           TEMP1_DEVICE_FILE_NAME,         TEMP1_DEV_ID},
    {DEV_DIR,           ACCEL_DEVICE_FILE_NAME,         ACCEL_DEV_ID},
    {DEV_DIR,           MAG_DEVICE_FILE_NAME,           MAG_DEV_ID},
    {DEV_DIR,           TAMPER_0_DEVICE_FILE_NAME,      TAMPER_0_DEV_ID},
    {DEV_DIR,           TAMPER_1_DEVICE_FILE_NAME,      TAMPER_1_DEV_ID},
    {DEV_DIR,           BOOT_REASON_DEVICE_FILE_NAME,   BOOT_REASON_DEV_ID},
};
#define NUM_OF_DEVICES (sizeof(devices)/sizeof(device_t))

int init(void)
{
    int res;

    common.prev_status_valid = 0;
    common.msp_broken = 0;

    res = ps_log_init_default();
    if (res < 0) {
        fprintf(stderr, LOG_TAG "ps_log_init_default failed with %d\n", res);
        return -1;
    }

    if (common.syslog)
        openlog("lemond", LOG_NDELAY, LOG_DAEMON);

    res = mutexCreate(&common.mmp_lock);
    if (res != EOK) {
        log_error("failed to create mmp lock (%d)", res);
        return -1;
    }

    res = condCreate(&common.mmp_tx_idle);
    if (res != EOK) {
        log_error("failed to create tx idle cond (%d)", res);
        return -1;
    }

    res = serial_init(SERIAL_DEV_NAME);
    if (res < 0) {
        log_error("failed to initialize serial (%d)", res);
        return -1;
    }

    res = mmp_init(&common.mmp, mmp_read, mmp_write, mmp_rx_handler);
    if (res < 0)
        return -1;

    beginthread(mmp_thread, common.mmp_thd_prio, mmp_stack, MMP_THD_STACK, NULL);

    /* Set system time */
    res = rtc_to_sys_clock();
    if (res < 0)
        log_warn("failed to set system time");

    /* Get boot reason before possible firmware update */
    common.boot_reason = get_boot_reason();

    /* Check firmware version */
    common.initial_version_unknown = (get_firmware_version(&common.initial_version) != 0);
    int upgrade_needed = (common.initial_version_unknown || !is_firmware_up_to_date(&common.initial_version));

    /* Firmware upgrade */
    if (upgrade_needed && is_firmware_upgrade_allowed()) {

        log_info("starting MSP firmware update");

        res = firmware_update();
        if (res < 0) {
            log_error("firmware update failed (%d)", res);
            return -1;
        }

        log_info("MSP firmware successfully updated");

        res = sys_clock_to_rtc();
        if (res < 0) {
            log_error("failed to set RTC time after firmware update", res);
            return -1;
        }
    }

    res = portCreate(&common.port);
    if (res != EOK) {
        log_error("could not create port (%d)", res);
        return -1;
    }

    unsigned i;
    for (i = 0; i < NUM_OF_DEVICES; i++) {
        res = dev_init(devices[i].dir, devices[i].file_name, devices[i].dev_id);
        if (res < 0) {
            log_error("failed to initialize %s/%s (%d)", devices[i].dir, devices[i].file_name, res);
            return -1;
        }
    }

    // log_info("initialized");

    return 0;
}

int main(int argc, char *argv[])
{
    int res, display_usage = 0;
    oid_t root;
    mmp_status_t status;

    common.display_msp_logs = 0;
    common.log_level = LOG_INFO;

    common.mmp_thd_prio = -1;
    common.worker_prio = -1;
    common.main_prio = -1;

    common.last_accel_alarm = 0;
    common.keep_accel_alarm_for = 5; // default value

    common.updated_at = 0;
    common.update_event_pending = 0;

    common.boot_reason = BOOT_REASON__UNKNOWN;

    while ((res = getopt(argc, argv, "dvsp:H:")) >= 0) {
        switch (res) {
        case 'd':
            common.display_msp_logs = 1;
            break;
        case 'v':
            common.log_level = LOG_DEBUG;
            break;
        case 's':
            common.syslog = 1;
            break;
        case 'p':
            common.mmp_thd_prio = common.worker_prio = common.main_prio = (int)strtol(optarg, NULL, 0);
            break;
        case 'H':
            common.mmp_thd_prio = common.worker_prio = common.keep_accel_alarm_for = (int)strtol(optarg, NULL, 0);
            break;
        default:
            display_usage = 1;
            break;
        }
    }

    /* Set defaults */
    if (common.mmp_thd_prio < 0) common.mmp_thd_prio = MMP_THD_PRIO;
    if (common.worker_prio < 0)  common.worker_prio  = WORKER_THD_PRIO;
    if (common.main_prio < 0)    common.main_prio    = MAIN_THD_PRIO;

    if (display_usage) {
        printf("Usage: lemond [-dvs][-p prio]\n\r");
        printf("    -d      Display MSP logs\n\r");
        printf("    -v      Verbose\n\r");
        printf("    -s      Output logs to syslog instead of stdout\n\r");
        printf("    -p prio Set priority (all threads)\n\r");
        printf("    -H secs How long to hold accelerometer alarm for (default: %llu)\n\r", common.keep_accel_alarm_for);
        return 1;
    }

    /* Wait for the filesystem */
    while (lookup("/", NULL, &root) < 0)
        usleep(10000);

    if (init() < 0) {
        log_error("initialization failed");
        create_flag_file(MSP_BROKEN_FILE);
        for(;;)
            usleep(1000*1000);
    }

    priority(common.main_prio);

    beginthread(worker_thread, common.worker_prio, worker_stack, WORKER_THD_STACK, NULL);

    while (1) {

        if (!common.dcsap_available) {
            if (ps_dcsap_logger_try_connect() >= 0) {
                common.dcsap_available = 1;
                log_info("DCSAP server is available");
            }
        } else if (common.update_event_pending) {
            res = add_firmware_update_event();
            if (res == 0)
                common.update_event_pending = 0;
        }

        /* Watchodog refresh */
        log_debug("refreshing WDG...");
        res = mmp_send(&common.mmp, MMP_CMD__WDG_REFRESH, NULL, 0, NULL, 0);
        if (res < 0) {
            log_error("failed to refresh WDG (res=%d)");
        } else {
            log_debug("WDG refreshed");
        }

        /* Get status */
        if (res == MMP_RES__OK)
            res = mmp_send(&common.mmp, MMP_CMD__GET_STATUS, NULL, 0, (void*)&status, sizeof(mmp_status_t));

        /* Enable/disable reporting events based on the DCSAP connection status */
        if (res == MMP_RES__OK) {
            if (!status.sendingEventsEnabled && common.dcsap_available) {
                log_debug("enabling reporting events");
                res = mmp_send(&common.mmp, MMP_CMD__ENABLE_PUSHING_EVENTS, NULL, 0, NULL, 0);
            } else if (status.sendingEventsEnabled && !common.dcsap_available) {
                log_debug("disabling reporting events");
                res = mmp_send(&common.mmp, MMP_CMD__DISABLE_PUSHING_EVENTS, NULL, 0, NULL, 0);
            }
        }

        if (res < 0) {
            if (!common.msp_broken) {
                log_error("communication with MSP lost (%d)", res);
                common.msp_broken = 1;
                create_flag_file(MSP_BROKEN_FILE);
            }

        } else {
            if (common.msp_broken) {
                log_info("MSP is responding again");
                common.msp_broken = 0;
                destroy_flag_file(MSP_BROKEN_FILE);
            }

            update_status(status);
        }

        usleep(5000*1000);
    }

    /* Should never be reached */
    log_error("Exiting!");

    return 0;
}
