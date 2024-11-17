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
 * @brief I2C driver control events IDs
*/
typedef enum {
    I2CDRV_EVENT_ATTACH,       /*!< [in] Attach new device request */
    I2CDRV_EVENT_DEATTACH,     /*!< [in] Deattach device request */
    I2CDRV_EVENT_OPEXEC,
    I2CDRV_EVENT_DUMP = 0xFF   /* For test only */
} i2cdrv_cmnd_event_t;

ESP_EVENT_DECLARE_BASE(I2CCMND_EVENT);

/**
 * @brief I2C driver control events IDs
*/
typedef enum {
    I2CDRV_EVENT_ATTACHED,     /*!< [out] Device attached to I2C Bus */
    I2CDRV_EVENT_DEATTACHED,   /*!< [out] Device deattached from I2C Bus */
    I2CDRV_EVENT_DATA,
    I2CDRV_EVENT_ERROR
} i2cdrv_resp_event_t;

ESP_EVENT_DECLARE_BASE(I2CRESP_EVENT);

typedef enum {
    BUSCMD_READ,
    BUSCMD_WRITE,
    BUSCMD_RW,
    BUSCMD_PROBE,
    BUSCMD_CTL
} i2cdrv_bus_command_t;

typedef enum {
    BUSDATA_BLOB = 0,
    BUSDATA_UINT8,
    BUSDATA_UINT16,
    BUSDATA_UINT32,
    BUSDATA_UINT64,
    BUSDATA_MAX
} i2cdrv_bus_data_t;

typedef enum {
    BUS_ERR_NOT_FOUND,
    BUS_ERR_TIMEOUT,
    BUS_ERR_BAD_ARGS,
    BUS_ERR_UNKNOWN,
    BUS_OK,
    ERR_NO_MEM,
    ERR_NO_MORE_BUSES,
    ERR_PIN_IN_USE,
    ERR_DEVICE_ALREADY_ATTACHED,
    ERR_DEVICE_NOT_FOUND,
    ERR_DEVICE_NOT_ACK
} i2cdrv_bus_opcodes_t;

/**
 * @brief Type of unique ID for attached i2c device
*/
typedef union {
    struct {
        uint32_t i2caddr:10;    /*!< Device I2C Address */
        uint32_t i2cbus:2;      /*!< Controller bus number */
        uint32_t gpiosda:7;     /*!< Bus SDA GPIO */
        uint32_t gpioscl:7;     /*!< Bus SDA GPIO */
        uint32_t reserved:6;    /*!< Not used */
    };
    uint32_t id;                    /*!< Single i2c device ID */
} i2cdrv_device_id_t;

typedef struct {
    union {
        struct {
            uint32_t cmd:3;
            uint32_t type:3;
            uint32_t inDataLen:7;
            uint32_t outDataLen:7;
            uint32_t code:4;
            uint32_t reserved:8;
        };
        uint32_t holder;
    };

    i2cdrv_device_id_t device_id;
    uint32_t event_id;
    uint8_t payload[127];
    //uint8_t InData[127];
    //uint8_t OutData[127];
} i2cdrv_comm_event_data_t;

/**
 * @brief Type of i2c device configuration
*/
typedef struct i2cdrv_device_config {
    i2c_device_config_t dev_config;     /*!< Device configuration */
    gpio_num_t sda_io_num;              /*!< GPIO for SDA I2C Signal */
    gpio_num_t scl_io_num;              /*!< GPIO for SCL I2C Signal */
} i2cdrv_device_config_t;              /*!< GPIO for SDA I2C Signal */

esp_event_loop_handle_t *i2cdrv_init();

void i2v_priv_drv_dummy(void);

#endif
