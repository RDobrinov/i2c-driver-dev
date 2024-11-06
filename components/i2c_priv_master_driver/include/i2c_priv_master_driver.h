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
typedef struct i2c_pmd_device_config {
    i2c_device_config_t idf_config;     /*!< Device configuration */
    gpio_num_t sda_io_num;              /*!< GPIO for SDA I2C Signal */
    gpio_num_t scl_io_num;              /*!< GPIO for SCL I2C Signal */
} i2c_pmd_device_config_t;              /*!< GPIO for SDA I2C Signal */

/**
 * @brief Type of slist element for active i2c buses
*/
typedef struct i2c_pmd_bus_list {
    i2c_master_bus_handle_t bus_handle; /*!< Master bus handle */
    struct i2c_pmd_bus_list *next;      /*!< Pointer to next element */
} i2c_pmd_bus_list_t;

/**
 * @brief Type of button control and notification event IDs
*/
typedef enum {
    PRIVI2C_EVENT_ATTACH,       /*!< [in] Attach new device request */
    PRIVI2C_EVENT_DEATTACH,     /*!< [in] Deattach device request */
    PRIVI2C_EVENT_ATTACHED,     /*!< [out] Send when driver registred a button    */
    PRIVI2C_EVENT_DEATTACHED    /*!< [out] Send when driver registred a button    */
} i2c_pmd_driver_event_t;

ESP_EVENT_DECLARE_BASE(PRIVI2C_EVENT);

void i2c_priv_drv_init(esp_event_loop_handle_t *i2cdrv_evt_loop);

void i2v_priv_drv_dummy(void);

#endif