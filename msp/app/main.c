/*
 * Monitor application
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#include <string.h>

#include <hal.h>
#include <serial.h>
#include <log.h>
#include <gpio.h>
#include <rtc.h>
#include <spi.h>
#include <adc.h>

#include <lis3mdl.h>
#include <lis2dh12.h>
#include <fm25l04b.h>
#include <tampers.h>

#include <msp_mon_prot.h>
#include <imxwdg.h>
#include <event.h>
#include <state.h>

#include <board.h>

#include "version.h"

static mmp_t mmp;
static mmp_status_t mmp_status = {
    .accel      = MMP_STATUS__OK,
    .mag        = MMP_STATUS__OK,
    .fram       = MMP_STATUS__OK,
    .event      = MMP_STATUS__OK,
    .log        = MMP_STATUS__OK,
    .tampers    = MMP_STATUS__OK,
};

static int lowPowerMode;

static int init(void);

static int powerModeUpdate(void);
static int switchToLowPowerMode(void);
static int switchToNormalMode(void);
static void enterStandbyMode(void);
static void exitStandbyMode(void);
static void powerStatusUpdate(void);

static int mmp_write(const uint8_t *data, uint16_t len);
static int mmp_read(uint8_t *byte);
static int mmp_rx_handler(uint8_t cmd,
                          const uint8_t *data,
                          uint16_t data_len,
                          uint8_t *resp,
                          uint16_t *resp_len);

/* Firmware version */
static const mmp_version_t fw_version = {
    .major  = MSP_FW_VERSION_MAJOR,
    .minor  = MSP_FW_VERSION_MINOR,
    .patch  = MSP_FW_VERSION_PATCH,
};

static mmp_host_boot_reason_t host_boot_reason = MMP_HOST_BOOT_REASON__PWR;

int main(void)
{
    int res;

    res = init();
    if (res < 0)
        goto critical_error;

    /* Log reset reason */
    const char *resetReason;
    while ((resetReason = hal_getResetReasonAsString()) != NULL)
        log_info("At least one reset caused by: %s", resetReason);

    for(;;) {

        if (lowPowerMode) {
            /* Exit standby mode. Initialize peripherals and devices used in
             * low power mode. */
            exitStandbyMode();
        }

        board_extWdgRefresh();

        res = powerModeUpdate();
        if (res < 0)
            goto critical_error;

        /* Accelerometer */
        if (mmp_status.accel == MMP_STATUS__OK) {
            res = lis2dh12_update();
            if (res < 0) {
                mmp_status.accel = MMP_STATUS__UPDATE_ERROR;
                log_error("accelerometer update failed");
            }
        }

        /* Tampers */
        if (mmp_status.tampers == MMP_STATUS__OK) {
            res = tampers_update();
            if (res < 0) {
                mmp_status.tampers = MMP_STATUS__UPDATE_ERROR;
                log_error("tampers update failed");
            }
        }

        powerStatusUpdate();

        if (!lowPowerMode) {

            res = imxwdg_update();
            if (res != 0) /* IMX was reset */
                host_boot_reason = MMP_HOST_BOOT_REASON__WDG;

            /* Magnetometer */
            if (mmp_status.mag == MMP_STATUS__OK) {
                res = lis3mdl_update();
                if (res < 0) {
                    mmp_status.mag = MMP_STATUS__UPDATE_ERROR;
                    log_error("magnetometer update failed");
                }
            }

            /* Event subsystem */
            if (mmp_status.event == MMP_STATUS__OK) {
                res = event_update();
                if (res < 0) {
                    mmp_status.event = MMP_STATUS__UPDATE_ERROR;
                    log_error("event update failed");
                }
            }

            /* Communication with IMX */
            res = mmp_update(&mmp);
            if (res < 0)
                goto critical_error;
        }


        /* Log subsystem */
        if (mmp_status.log == MMP_STATUS__OK) {
            do {
                res = log_update();
            } while (res == LOG_CONTINUE_UPDATE);
            if (res < 0)
                mmp_status.log = MMP_STATUS__UPDATE_ERROR;
        }

        /* Check for clock faults */
        if (hal_clock32kHzFault()) {
            mmp_status.clock32kHz = MMP_STATUS__GENERAL_ERROR;
        } else {
            mmp_status.clock32kHz = MMP_STATUS__OK;
        }

        /* Store current device state in FRAM */
        res = state_store();
        if (res < 0)
            log_error("failed to store device state");

        if (lowPowerMode)
            enterStandbyMode();
    }

    return 0;

critical_error:

    /* TODO: What to do in case of critical error? Reset? */
    for(;;);
}

static int init(void)
{
    int res;

    /* Initialize only as much as would be initialized if we were exiting
     * standby mode and pretend that we're exiting standby mode */
    lowPowerMode = 1;

    hal_init();
    board_init();
    rtc_init();
    log_init();
    tampers_init();

    mmp_set_default_instance(&mmp);

    if (lis2dh12_init() < 0) {
        mmp_status.accel = MMP_STATUS__INIT_ERROR;
        log_error("accelerometer initialization failed");
    }

    if (fm25l04b_init() < 0) {
        mmp_status.fram = MMP_STATUS__INIT_ERROR;
        log_error("external FRAM initialization failed");
    }

    res = state_tryToRestore();
    if (res == STATE_RES__INVALID_STATE) {
        log_warn("failed to restore device state (possibly first run)");
    } else if (res != STATE_RES__OK) {
        log_error("failed to restore device state (%d)", res);
    }

    if (lis3mdl_init(LIS3MDL_MD_POWER_DOWN) < 0) {
        mmp_status.mag = MMP_STATUS__INIT_ERROR;
        log_error("magnetometer initialization failed");
    }

    if (event_init(mmp_status.fram == MMP_STATUS__OK) < 0) {
        mmp_status.event = MMP_STATUS__INIT_ERROR;
        log_error("failed to initialize event subsystem");
    }

    event_addNow(EVENT_MSP_RESET);

    return 0;
}

static int powerModeUpdate(void)
{
    uint32_t primary = board_getPrimaryVoltage();
    uint32_t auxiliary = board_getSecondaryVoltage();

    if (lowPowerMode && (primary >= VOLTAGE_THR_LPM_TO_NM || auxiliary >= VOLTAGE_THR_LPM_TO_NM)) {
        return switchToNormalMode();

    } else if (!lowPowerMode && (primary < VOLTAGE_THR_NM_TO_LPM && auxiliary < VOLTAGE_THR_NM_TO_LPM)) {
        return switchToLowPowerMode();
    }

    return 0;
}

static int switchToLowPowerMode(void)
{
    if (lis3mdl_powerDown() < 0) {
        mmp_status.mag = MMP_STATUS__GENERAL_ERROR;
        log_error("failed to power down magnetometer");
    }

    mmp_deinit(&mmp);

    serial_deinit();

    log_debug("[%05u]: MSP running in low power mode.", rtc_getSysTime());
    lowPowerMode = 1;

    return 0;
}

static int switchToNormalMode()
{
    int res;

    serial_init(serial_baudrate__115200);

    res = mmp_init(&mmp, mmp_read, mmp_write, mmp_rx_handler);
    if (res < 0)
        return -1;

    /* Disable TX and event reporting for now. We'll enable both after
     * receiving first valid packet. */
    mmp_disable_tx(&mmp);
    event_disableSending();

    if (lis3mdl_powerUp() < 0) {
        mmp_status.mag = MMP_STATUS__GENERAL_ERROR;
        log_error("failed to power up magnetometer");
    }

    /* TODO: Add special grace period for IMX to boot? */
    imxwdg_init();

    log_debug("[%05u]: MSP running in normal mode.", rtc_getSysTime());
    lowPowerMode = 0;

    return 0;
}

static void enterStandbyMode(void)
{
    spi_deinit(0);
    spi_deinit(1);

    adc_deinit();

    board_resetPinConfig();

    hal_enterStandbyMode();
}

static void exitStandbyMode(void)
{
    spi_cfg_t spi_cfg;
    spi_cfg.msb_first = 1;

    spi_cfg.cpha = 1;
    spi_cfg.cpol = 0;
    spi_init(0, &spi_cfg);

    spi_cfg.cpha = 0;
    spi_cfg.cpol = 1;
    spi_init(1, &spi_cfg);

    adc_init();
}

static void powerStatusUpdate(void)
{
    uint32_t voltage;

    /* Battery */
    unsigned battery_power_fail;
    state_getFlag(mmp_state_flag__battery_fail, &battery_power_fail);
    voltage = board_getBatteryVoltage();
    if (voltage < (LOW_BATTERY_VOLTAGE_THR - LOW_BATTERY_VOLTAGE_HYST) && !battery_power_fail) {
        event_addNow(EVENT_BATTERY_LOW);
        battery_power_fail = 1;
    }
    if (voltage > (LOW_BATTERY_VOLTAGE_THR + LOW_BATTERY_VOLTAGE_HYST) && battery_power_fail) {
        event_addNow(EVENT_BATTERY_OK);
        battery_power_fail = 0;
    }
    state_setFlag(mmp_state_flag__battery_fail, battery_power_fail);

    /* Main power */
    unsigned main_power_fail;
    state_getFlag(mmp_state_flag__main_power_fail, &main_power_fail);
    voltage = board_getPrimaryVoltage();
    if (voltage < (PRIMARY_VOLTAGE_THR - PRIMARY_VOLTAGE_HYST) && !main_power_fail) {
        event_addNow(EVENT_MAIN_POWER_OUTAGE);
        main_power_fail = 1;
    }
    if (voltage > (PRIMARY_VOLTAGE_THR + PRIMARY_VOLTAGE_HYST) && main_power_fail) {
        event_addNow(EVENT_MAIN_POWER_BACK);
        main_power_fail = 0;
    }
    state_setFlag(mmp_state_flag__main_power_fail, main_power_fail);

    /* Auxiliary power */
    unsigned aux_power_fail;
    state_getFlag(mmp_state_flag__aux_power_fail, &aux_power_fail);
    voltage = board_getSecondaryVoltage();
    if (voltage < (SECONDARY_VOLTAGE_THR - SECONDARY_VOLTAGE_HYST) && !aux_power_fail) {
        event_addNow(EVENT_AUX_POWER_OUTAGE);
        aux_power_fail = 1;
    }
    if (voltage > (SECONDARY_VOLTAGE_THR + SECONDARY_VOLTAGE_HYST) && aux_power_fail) {
        event_addNow(EVENT_AUX_POWER_BACK);
        aux_power_fail = 0;
    }
    state_setFlag(mmp_state_flag__aux_power_fail, aux_power_fail);

    if (main_power_fail && aux_power_fail) /* Both power supplies of host processor failed */
        host_boot_reason = MMP_HOST_BOOT_REASON__PWR;
}

static int mmp_write(const uint8_t *data, uint16_t len)
{
    return serial_write(data, len);
}

static int mmp_read(uint8_t *byte)
{
    return serial_read(byte, 1);
}

static int mmp_rx_handler(uint8_t cmd,
                          const uint8_t *data,
                          uint16_t data_len,
                          uint8_t *resp,
                          uint16_t *resp_len)
{
    int res, idx = -1;
    mmp_time_t t;
    mmp_voltage_t v;
    mmp_temperature_t temp;
    mmp_nack_t nack;
    mmp_state_flags_t state_flags;

    switch (cmd) {
    case MMP_CMD__GET_STATUS:
        mmp_status.sendingEventsEnabled = event_isSendingEnabled();
        memcpy(resp, &mmp_status, sizeof(mmp_status_t));
        *resp_len = sizeof(mmp_status_t);
        break;

    case MMP_CMD__GET_TIME:
        do {
            res = rtc_getUnixTime(&t.unix_time);
        } while (res != 0);
        memcpy(resp, &t, sizeof(mmp_time_t));
        *resp_len = sizeof(mmp_time_t);
        break;

    case MMP_CMD__SET_TIME:
        if (data_len != sizeof(mmp_time_t)) {
            nack.error_code = MMP_RES__INVALID_PACKET;
            goto send_nack;
        }
        memcpy(&t, data, sizeof(mmp_time_t));
        rtc_setUnixTime(t.unix_time);
        *resp_len = 0;
        break;

    case MMP_CMD__WDG_REFRESH:
        imxwdg_refresh();
        *resp_len = 0;
        break;

    case MMP_CMD__GET_VBAT:
        v.voltage = board_getBatteryVoltage();
        memcpy(resp, &v, sizeof(mmp_voltage_t));
        *resp_len = sizeof(mmp_voltage_t);
        break;

    case MMP_CMD__GET_VPRI:
        v.voltage = board_getPrimaryVoltage();
        memcpy(resp, &v, sizeof(mmp_voltage_t));
        *resp_len = sizeof(mmp_voltage_t);
        break;

    case MMP_CMD__GET_VSEC:
        v.voltage = board_getSecondaryVoltage();
        memcpy(resp, &v, sizeof(mmp_voltage_t));
        *resp_len = sizeof(mmp_voltage_t);
        break;

    case MMP_CMD__GET_TEMP0:
    case MMP_CMD__GET_TEMP1:
        if (cmd == MMP_CMD__GET_TEMP0)
            idx = 0;
        else
            idx = 1;
        temp.temp = board_getTemperature(idx);
        memcpy(resp, &temp, sizeof(mmp_temperature_t));
        *resp_len = sizeof(mmp_temperature_t);
        break;

    case MMP_CMD__GET_STATE_FLAGS:
        state_flags = state_get();
        memcpy(resp, &state_flags, sizeof(mmp_state_flags_t));
        *resp_len = sizeof(mmp_state_flags_t);
        break;

    case MMP_CMD__ENABLE_PUSHING_EVENTS:
        event_enableSending();
        *resp_len = 0;
        break;

    case MMP_CMD__DISABLE_PUSHING_EVENTS:
        event_disableSending();
        *resp_len = 0;
        break;

    case MMP_CMD__GET_VERSION:
        memcpy(resp, &fw_version, sizeof(mmp_version_t));
        *resp_len = sizeof(mmp_version_t);
        break;

    case MMP_CMD__GET_BOOT_REASON:
        memcpy(resp, &host_boot_reason, sizeof(host_boot_reason));
        *resp_len = sizeof(host_boot_reason);
        break;

    case MMP_CMD__ENTER_BOOTLOADER:
        hal_enterBootloader();

        /* Should not be reached */
        nack.error_code = MMP_RES__CMD_EXECUTION_ERROR;
        goto send_nack;

    default:
        nack.error_code = MMP_RES__UNSUPPORTED_CMD;
        goto send_nack;
    }

    /* We have communication with IMX. Enable TX. */
    mmp_enable_tx(&mmp);

    return MMP_RES__OK;

send_nack:
    *resp_len = sizeof(mmp_nack_t);
    memcpy(resp, &nack, sizeof(mmp_nack_t));

    return MMP_RES__NACK;
}
