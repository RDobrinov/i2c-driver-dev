/*
 * SPDX-FileCopyrightText: 2024 Rossen Dobrinov
 *
 * SPDX-License-Identifier: Internal use only
 */

/*
 *
*/

#ifndef _I2C_PRIV_MASTER_DRIVER_
#define _I2C_PRIV_MASTER_DRIVER_

#include "driver/i2c_master.h"
#include "esp_event.h"

/**
 * @brief Type of i2c device configuration
*/
typedef struct i2cdrv_device_config {
    i2c_device_config_t dev_config;     /*!< Device configuration */
    gpio_num_t sda_io_num;              /*!< GPIO for SDA I2C Signal */
    gpio_num_t scl_io_num;              /*!< GPIO for SCL I2C Signal */
} i2cdrv_device_config_t;              /*!< GPIO for SDA I2C Signal */

/**
 * @brief I2C driver control events IDs
*/
typedef enum {
    I2CDRV_EVENT_ATTACH,       /*!< [in] Attach new device request */
    I2CDRV_EVENT_DEATTACH,     /*!< [in] Deattach device request */
    I2CDRV_EVENT_DUMP = 0xFF   /* For test only */
} i2cdrv_cmnd_event_t;

ESP_EVENT_DECLARE_BASE(I2CCMND_EVENT);

/**
 * @brief I2C driver control events IDs
*/
typedef enum {
    I2CDRV_EVENT_ATTACHED,     /*!< [out] Device attached to I2C Bus */
    I2CDRV_EVENT_DEATTACHED,   /*!< [out] Device deattached from I2C Bus */
    I2CDRV_EVENT_ERROR
} i2cdrv_resp_event_t;

ESP_EVENT_DECLARE_BASE(I2CRESP_EVENT);

typedef enum {
    DRV_ERR_ATTACH_FAILED,
    DRV_ERR_DEATTACH_FAILED
} i2cdrv_error_codes_t;

typedef enum {
    DRV_ERR_NO_MORE_BUSES,
    DRV_ERR_PIN_ALREADY_USED,
    DRV_ERR_DEVICE_ALREADY_ATTACHED,
    DRV_ERR_DEVICE_NOT_FOUND
} i2cdrv_error_subcodes_t;

/**
 * @brief Event data with error codes
*/
typedef union {
    struct {
        uint32_t subcode:16;    /*!< Error subcode */
        uint32_t code:16;       /*!< Error code */
    };
    uint32_t id;                /*!< Codes holder */
} i2cdrv_error_event_t;

void i2cdrv_init(esp_event_loop_handle_t *i2cdrv_evt_loop);

void i2v_priv_drv_dummy(void);

#endif