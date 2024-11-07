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

    i2c_pmd_bus_list_t *bus_list = (i2c_pmd_bus_list_t *)calloc(1, sizeof(i2c_pmd_bus_list_t));
    i2c_pmd_bus_list_t *bus_element;

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = GPIO_NUM_18,
        .sda_io_num = GPIO_NUM_19,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&i2c_mst_config, &(bus_list->bus_handle));

    if( err != ESP_OK ) {
        free(bus_list);
        bus_list = NULL;
    }

    if(bus_list) {
        i2c_mst_config.scl_io_num = GPIO_NUM_21;
        i2c_mst_config.sda_io_num = GPIO_NUM_22;

        bus_element = (i2c_pmd_bus_list_t *)calloc(1, sizeof(i2c_pmd_bus_list_t));
        err = i2c_new_master_bus(&i2c_mst_config, &(bus_element->bus_handle));

        if( err != ESP_OK ) {
            free(bus_element);
            bus_element = NULL;
        }

        if( bus_element ) 
        {
            bus_element->next = bus_list;
            bus_list = bus_element;
        }

        i2c_mst_config.scl_io_num = GPIO_NUM_23;
        i2c_mst_config.sda_io_num = GPIO_NUM_25;

        bus_element = (i2c_pmd_bus_list_t *)calloc(1, sizeof(i2c_pmd_bus_list_t));
        err = i2c_new_master_bus(&i2c_mst_config, &(bus_element->bus_handle));

        if( err != ESP_OK ) {
            if( err == ESP_ERR_NOT_FOUND ) { printf("No more free buses\n"); }
            free(bus_element);
            bus_element = NULL;
        }

        if( bus_element ) 
        {
            bus_element->next = bus_list;
            bus_list = bus_element;
        }
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x21,
        .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t dev_handle;

    bus_element = bus_list;
    if(bus_element) {
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_element->bus_handle, &dev_cfg, &dev_handle));
        dev_cfg.device_address = 0x22;
        dev_cfg.scl_speed_hz = 200000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_element->bus_handle, &dev_cfg, &dev_handle));
        dev_cfg.device_address = 0x23;
        dev_cfg.scl_speed_hz = 300000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_element->bus_handle, &dev_cfg, &dev_handle));
    }
    bus_element = bus_element->next;
    if(bus_element) {
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_element->bus_handle, &dev_cfg, &dev_handle));
        dev_cfg.device_address = 0x22;
        dev_cfg.scl_speed_hz = 200000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_element->bus_handle, &dev_cfg, &dev_handle));
        dev_cfg.device_address = 0x21;
        dev_cfg.scl_speed_hz = 100000;
        ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_element->bus_handle, &dev_cfg, &dev_handle));
    }
    //Bus structures located in i2c_private.h
    bus_element = bus_list; 
    while(bus_element) {
        printf("[I2C%02u] GPIO%02u[SDA] GPIO%02u[SCL] CLK_SRC %u, CLK_SRC_FREQ %lu, PULLUP %u, I2C_TRANS_QSIZE %u\n", 
            ((i2c_bus_t *)(bus_element->bus_handle->base))->port_num,
            ((i2c_bus_t *)(bus_element->bus_handle->base))->sda_num,
            ((i2c_bus_t *)(bus_element->bus_handle->base))->scl_num,
            ((i2c_bus_t *)(bus_element->bus_handle->base))->clk_src,
            ((i2c_bus_t *)(bus_element->bus_handle->base))->clk_src_freq_hz,
            ((i2c_bus_t *)(bus_element->bus_handle->base))->pull_up_enable,
            bus_element->bus_handle->queue_size
        );
        i2c_master_device_list_t *bus_devices = bus_element->bus_handle->device_list.slh_first;
        if(bus_devices) printf("Devices attached to bus\n");
        while(bus_devices) {
            printf("0x%02x@%luHz 10Bits %u ACK %u\n",
            bus_devices->device->device_address, bus_devices->device->scl_speed_hz, bus_devices->device->addr_10bits, bus_devices->device->ack_check_disable);
            bus_devices = (i2c_master_device_list_t*) bus_devices->next.sle_next;
        }
        bus_element = bus_element->next;
    };

    for (int i = 10; i >= 0; i--) {
        printf("Restarting in %d seconds...\n", i);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    printf("Stop.\n");
    fflush(stdout);
}
