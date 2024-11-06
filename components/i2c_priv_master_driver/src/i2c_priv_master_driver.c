/*
 * SPDX-FileCopyrightText: 2024 Rossen Dobrinov
 *
 * SPDX-License-Identifier: Internal use only
 */

/*
 * i2c_private.h
*/

#include "i2c_priv_master_driver.h"

/**
 * @brief Type of unique ID for attached i2c device
*/
typedef union {
    struct {
        uint32_t i2caddr:10;        /*!< Device I2C Address */
        uint32_t i2cbus:2;          /*!< Controller bus number */
        uint32_t reserved_lsw:4;    /*!< Not used */
        uint32_t gpiosda:7;         /*!< Bus SDA GPIO */
        uint32_t gpioscl:7;         /*!< Bus SDA GPIO */
        uint32_t reserved_msw:2;    /*!< Not used */
    };
    uint32_t id;                    /*!< Single i2c device ID */
} i2c_pmd_device_id_t;


void i2c_priv_drv_init(esp_event_loop_handle_t *i2cdrv_evt_loop) {
    
}

void i2v_priv_drv_dummy(void) {
    return;
}