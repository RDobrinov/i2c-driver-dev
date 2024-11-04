/*
 * SPDX-FileCopyrightText: 2024 Rossen Dobrinov
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Copyright 2024 Rossen Dobrinov
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _GPIO_DRIVER_H_
#define _GPIO_DRIVER_H_

#include <driver/gpio.h>
#include "driver/rtc_io.h"
#include "esp_bit_defs.h"
#include "hal/gpio_hal.h"
#include "hal/misc.h"
#include "soc/rtc_io_channel.h"
#include "soc/rtc_io_periph.h"
#include "soc/rtc_io_struct.h"
#include "soc/soc.h"

/**
 * @brief Type of GPIO states
*/
typedef enum {
    GPIO_STATE_LOW = 0, /*!< GPIO Low */
    GPIO_STATE_HIGH = 1 /*!< GPIO High */
} gpio_drv_state_t;

/**
 * @brief Type of GPIO IO MCU config (IO_MUX_x_REGx or RTCIO)
*/
typedef struct gpio_pin_io_config {
    gpio_num_t gpio_num;        /*!< GPIO number */
    bool reserved;              /*!< GPIO driver reservation state      */
    bool rtc_mux;               /*!< RTC MUX enabled or not             */
    bool pull_up;               /*!< Pull-up enabled or not             */
    bool pull_down;             /*!< Pull-down enabled or not           */
    bool input_enable;          /*!< Input enabled or not               */
    bool output_enable;         /*!< Output enabled or not              */
    bool open_drain;            /*!< Open drain enabled or not          */
    bool sleep_enable;          /*!< Pin sleep mode enabled or not      */
    uint32_t drive_current;     /*!< Drive strength value of the pad    */
    uint32_t function_selected; /*!< Selected IO_MUX function           */
    uint32_t signal_out;        /*!< Outputting peripheral signal index */
} gpio_pin_io_config_t;

/**
 * @brief Create initial reservation state for all pads and fill states for all unusable GPIO.
*/
void gpio_drv_init(void);

/**
 * @brief Reserve single GPIO
 * 
 * @param[in] gpio_num GPIO_NUM_XX for reservation
 * @return 
 *      - TRUE GPIO is successfuls reserved for usage
 *      - FALSE GPIO is already reserved or GPIO is system/unusable
*/
bool gpio_drv_reserve(gpio_num_t gpio_num);

/**
 * @brief Reserve multiple GPIOs by bitmask
 * 
 * @param[in] pins_mask bitmask for reservation. Each bit represents a GPIO number for reservation
 * @return 
 *      - TRUE GPIOs are successfuly reserved for usage
 *      - FALSE One or more GPIOs are already reserved or GPIOs are system/unusable
*/
bool gpio_drv_reserve_pins(uint64_t pins_mask);

/**
 * @brief Free one GPIO 
 * 
 * @param[in] gpio_num GPIO_NUM_XX
 * @return 
 *      - TRUE GPIO is available for usage
 *      - FALSE GPIO is system/unusable
*/
bool gpio_drv_free(gpio_num_t gpio_num);

/**
 * @brief Free multiple GPIOs by bitmask.
 * 
 * @param[in] pins_mask bitmask. Each bit represents a GPIO number
 * @return 
 *      - TRUE GPIOs are available for usage
 *      - FALSE One or more GPIOs are system/unusable
*/
bool gpio_drv_free_pins(uint64_t pins_mask);

/**
 * @brief Check one GPIO for reservation status
 * 
 * @param[in] gpio_num GPIO_NUM_XX
 * @return 
 *      - TRUE GPIO is reserved or is system/unusable
 *      - FALSE GPIO is free
*/
bool gpio_drv_is_pin_reserved(gpio_num_t gpio_num);

/**
 * @brief Return internal bitmask for reservation status
 * 
 * @return 
 *      - 64 bits value. Each bit represents GPIO reservation status
*/
uint64_t gpio_drv_get_reservations(void);

/**
 * @brief Fill gpio_pin_io_config_t with current io pin configuration
 * 
 * @param[out] pin_io_config GPIO pin io configuration
*/
void gpio_drv_get_pin_io_config(gpio_pin_io_config_t *pin_io_config);

/**
 * @brief Check one GPIO for reservation status
 * 
 * @param[in] gpio_num GPIO_NUM_XX
 * @return 
 *      - Input signal connected to gpio_num
 *      - SIG_GPIO_OUT_IDX if not signal connected ( simple GPIO )
*/
uint32_t gpio_drv_get_in_signal(gpio_num_t gpio_num);

#ifdef CONFIG_GPIO_TEXT_FUNCTIONS
/**
 * @brief Get signal name as null terminated string
 * 
 * @param[in] signal In or Out signal index
 * @param[in] is_input True/False for Input/Output signal
 * @return 
 *      - Pointer to string
*/
char *gpio_drv_get_sig_name(uint32_t signal, bool is_input);

/**
 * @brief Get IOMUX Function name
 * 
 * @param[in] function IOMUX function selected
 * @param[in] gpio_num Direct connected GPIO_NUM_XX
 * @return 
 *      - Pointer to string
*/
char *gpio_drv_get_iomux_func_name(uint32_t function, gpio_num_t gpio_num);

/**
 * @brief Get friendly pin io configuration description as null terminated string
 * 
 * @param[in] gpio_num GPIO_NUM_XX
 * @param[in] short_desc True/False for Short/Long description
 * @return 
 *      - Pointer to string
*/
char *gpio_drv_get_io_description(gpio_num_t gpio_num, bool short_desc);
#endif /* CONFIG_GPIO_TEXT_FUNCTIONS */
#endif /* _GPIO_DRIVER_H_ */
