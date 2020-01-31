/*
 * Board configuration
 *
 * Copyright 2018 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Krystian Wasik
 *
 * %LICENSE%
 */

#ifndef BOARD_H_
#define BOARD_H_

#include <stdint.h>

#include <gpio.h>

/* External watchdog */
#define EXT_WDG_PORT                (6)
#define EXT_WDG_PIN                 (5)

/* Tampers */
#define TAMPER1_PORT                (5)
#define TAMPER1_PIN                 (2)
#define TAMPER1_OPEN_STATE          (1)

#define TAMPER2_PORT                (5)
#define TAMPER2_PIN                 (3)
#define TAMPER2_OPEN_STATE          (1)

/* FRAM (FM25L04B) */
#define FM25L04B_SPI_IDX            (0)

#define FM25L04B_CS_PORT            (4)
#define FM25L04B_CS_PIN             (6)

#define FM25L04B_PWRD_PORT          (4)
#define FM25L04B_PWRD_PIN           (3)

/* Accelerometer (LIS2DH12) */
#define LIS2DH12_SPI_IDX            (1)

#define LIS2DH12_CS_PORT            (1)
#define LIS2DH12_CS_PIN             (4)

/* Magnetometer (LIS3MDL) */
#define LIS3MDL_SPI_IDX             (1)

#define LIS3MDL_CS_PORT             (1)
#define LIS3MDL_CS_PIN              (5)

/* ADC channels */
#define VBAT_ADC_CHANNEL            (0)
#define VPRI_ADC_CHANNEL            (1)
#define VSEC_ADC_CHANNEL            (2)
#define TEMP_ADC_CHANNEL            (10)

#define IMX_RESET_PORT              (1)
#define IMX_RESET_PIN               (2)

#define IMX_ONOFF_PORT              (1)
#define IMX_ONOFF_PIN               (3)

#define PFO_PORT                    (1)
#define PFO_PIN                     (0)

/* Voltage threshold for switching from normal mode to low power mode */
#define VOLTAGE_THR_NM_TO_LPM       ((uint32_t)3000) /* mV */

/* Voltage threshold for switching from low power mode to normal mode */
#define VOLTAGE_THR_LPM_TO_NM       ((uint32_t)3000) /* mV */

/* Battery voltage threshold and hysteresis */
#define LOW_BATTERY_VOLTAGE_THR     ((uint32_t)3000) /* mV */
#define LOW_BATTERY_VOLTAGE_HYST    ((uint32_t)200) /* mV */

/* Primary voltage threshold and hysteresis  */
#define PRIMARY_VOLTAGE_THR         ((uint32_t)12000) /* mV */
#define PRIMARY_VOLTAGE_HYST        ((uint32_t)500) /* mV */

/* Secondary voltage threshold and hysteresis  */
#define SECONDARY_VOLTAGE_THR       ((uint32_t)12000) /* mV */
#define SECONDARY_VOLTAGE_HYST      ((uint32_t)500) /* mV */

void board_init(void);
void board_resetPinConfig(void);

uint32_t board_getBatteryVoltage(void);
uint32_t board_getPrimaryVoltage(void);
uint32_t board_getSecondaryVoltage(void);
int32_t board_getTemperature(int idx);

static inline void board_extWdgRefresh(void)
{
    gpio_toggle(EXT_WDG_PORT, EXT_WDG_PIN);
}

#endif /* BOARD_H_ */
