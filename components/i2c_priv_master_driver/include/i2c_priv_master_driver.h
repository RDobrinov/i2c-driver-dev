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

typedef struct i2c_pmd_device_config {
    i2c_device_config_t idf_config;
    gpio_num_t sda_io_num;
    gpio_num_t scl_io_num;
} i2c_pmd_device_config_t;

typedef struct i2c_pmd_bus_list {
    i2c_master_bus_handle_t bus_handle;
    struct i2c_pmd_bus_list *next;
} i2c_pmd_bus_list_t;

void i2v_priv_drv_dummy(void);

#endif