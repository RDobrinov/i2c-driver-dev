/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
/*
 * Bug in components/esp_i2c_driver/i2c_master.c and components/esp_i2c_driver/i2c_master.c
 * esp_err_t i2c_release_bus_handle(i2c_bus_handle_t i2c_bus) try to release NULL handle
 * i2c_common_deinit_pins(i2c_master->base); try to deinit pins with i2c_master->base NULL pointer
*/
#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"

#include "esp_err.h"

#include "driver/i2c_master.h"
#include "../i2c_private.h"
#include "i2c_priv_master_driver.h"

#include "esp_log.h"

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    uint32_t flash_size;
    esp_chip_info(&chip_info);
    printf("This is %s chip with %d CPU core(s), %s%s%s%s, ",
           CONFIG_IDF_TARGET,
           chip_info.cores,
           (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi/" : "",
           (chip_info.features & CHIP_FEATURE_BT) ? "BT" : "",
           (chip_info.features & CHIP_FEATURE_BLE) ? "BLE" : "",
           (chip_info.features & CHIP_FEATURE_IEEE802154) ? ", 802.15.4 (Zigbee/Thread)" : "");

    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    printf("silicon revision v%d.%d, ", major_rev, minor_rev);
    if(esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        printf("Get flash size failed");
        return;
    }

    printf("%" PRIu32 "MB %s flash\n", flash_size / (uint32_t)(1024 * 1024),
           (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    printf("Minimum free heap size: %" PRIu32 " bytes\n", esp_get_minimum_free_heap_size());

    /**/
    esp_event_loop_handle_t *uevent_loop = (esp_event_loop_handle_t *)malloc(sizeof(esp_event_loop_handle_t));
    esp_event_loop_args_t uevent_args = {
        .queue_size = 5,
        .task_name = "testuloop",
        .task_priority = 15,
        .task_stack_size = 3072,
        .task_core_id = tskNO_AFFINITY
    };
    esp_err_t err;
    err = esp_event_loop_create(&uevent_args, uevent_loop);
    (void)err;

    i2cdrv_init(uevent_loop);

    i2cdrv_device_config_t *test_dev = (i2cdrv_device_config_t *)calloc(1, sizeof(i2cdrv_device_config_t));
    test_dev->dev_config = (i2c_device_config_t) {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x21,
        .scl_speed_hz = 100000
    };
    test_dev->scl_io_num = GPIO_NUM_21;
    test_dev->sda_io_num = GPIO_NUM_22;
    ESP_LOGE("78","LOGE");
    esp_event_post_to(uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_ATTACH, test_dev, sizeof(i2cdrv_device_config_t), 1);

    /**/
    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Stop.\n");
    fflush(stdout);
}
