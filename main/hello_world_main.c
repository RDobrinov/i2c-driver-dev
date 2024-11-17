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

/* test */
#include "idf_gpio_driver.h"
/* test */
#include "driver/rmt_tx.h"

#include "esp_log.h"

static char *mtag = "main";

static const char *cc_errors[] = {
    "BUS_ERR_NOT_FOUND",
    "BUS_ERR_TIMEOUT",
    "BUS_ERR_BAD_ARGS",
    "BUS_ERR_UNKNOWN",
    "BUS_OK",
    "ERR_NO_MEM",
    "ERR_NO_MORE_BUSES",
    "ERR_PIN_IN_USE",
    "ERR_DEVICE_ALREADY_ATTACHED",
    "ERR_DEVICE_NOT_FOUND",
    "ERR_DEVICE_NOT_ACK",
    "ERR_TEST"
};

//uint32_t delete_id;

//uint32_t inData, outData;
esp_event_loop_handle_t *uevent_loop;
//uint8_t inBuffer[127];

void hexdump(const uint8_t *buf, size_t len) {
    if( !len ) return;
    ESP_LOGI("hexdump", "%p", buf);
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

static void main_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if( I2CRESP_EVENT == event_base) {
        if(I2CDRV_EVENT_ATTACHED == event_id) {
            //ESP_LOGI(mtag, "[I2CDRV_EVENT_ATTACHED] ID:%08lX @ I2CBUS%u", *((uint32_t *)event_data), (uint8_t)(0x03 & (*((uint32_t *)event_data)>>10)));
            ESP_LOGI(mtag, "[I2CDRV_EVENT_ATTACHED] ID:%08lX @ I2CBUS%u", ((i2cdrv_comm_event_data_t *)event_data)->device_id.id, ((i2cdrv_comm_event_data_t *)event_data)->device_id.i2cbus);
            ((i2cdrv_comm_event_data_t *)event_data)->cmd = BUSCMD_PROBE;
            esp_event_post_to(*uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_OPEXEC, event_data, sizeof(i2cdrv_comm_event_data_t), 1);
        }
        if(I2CDRV_EVENT_DEATTACHED == event_id) {
            ESP_LOGI(mtag, "[I2CDRV_EVENT_DEATTACHED] ID:%08lX @ I2CBUS%u", ((i2cdrv_comm_event_data_t *)event_data)->device_id.id, ((i2cdrv_comm_event_data_t *)event_data)->device_id.i2cbus);
        }
        if(I2CDRV_EVENT_ERROR == event_id) {
            //ESP_LOGI(mtag, "[I2CDRV_EVENT_ERROR] %05d", ((i2cdrv_comm_event_data_t *)event_data)->code);
            ESP_LOGI(mtag, "[EVTID: %04lX ERROR: %02d] %s ID: %08lX", ((i2cdrv_comm_event_data_t *)event_data)->event_id, 
                    ((i2cdrv_comm_event_data_t *)event_data)->code, 
                    cc_errors[((i2cdrv_comm_event_data_t *)event_data)->code],
                    ((i2cdrv_comm_event_data_t *)event_data)->device_id.id);
        }
        if(I2CDRV_EVENT_DATA == event_id) {
            if(((i2cdrv_comm_event_data_t *)event_data)->code != BUS_OK) {
                ESP_LOGE("event_handler", "I2CDRV_EVENT_DATA with no I2CDRV_BUS_OK");
                return;
            } else ESP_LOGI(mtag, "[I2CDRV_EVENT_DATA]");
            switch(((i2cdrv_comm_event_data_t *)event_data)->cmd) {
                case BUSCMD_READ:
                case BUSCMD_RW:
                    switch(((i2cdrv_comm_event_data_t *)event_data)->type) {
                        case BUSDATA_UINT8:
                            ESP_LOGI(mtag, "I2CDRV_BUSCMD_READ UINT8 %02X", *((uint8_t *)(((i2cdrv_comm_event_data_t *)event_data)->payload)));
                            break;
                        case BUSDATA_UINT16:
                            ESP_LOGI(mtag, "I2CDRV_BUSCMD_READ UINT16 %04X", *((uint16_t *)(((i2cdrv_comm_event_data_t *)event_data)->payload)));
                            break;
                        case BUSDATA_UINT32:
                            ESP_LOGI(mtag, "I2CDRV_BUSCMD_READ UINT32 %08lX", *((uint32_t *)(((i2cdrv_comm_event_data_t *)event_data)->payload)));
                            break;
                        case BUSDATA_UINT64:
                            ESP_LOGI(mtag, "I2CDRV_BUSCMD_READ UINT64 %16llX", *((uint64_t *)(((i2cdrv_comm_event_data_t *)event_data)->payload)));
                            break;
                        case BUSDATA_BLOB:
                            ESP_LOGI(mtag, "I2CDRV_BUSCMD_READ BLOB");
                            hexdump(((i2cdrv_comm_event_data_t *)event_data)->payload, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                            break;
                        default:
                            ESP_LOGE(mtag, "Unknow event maybe memory conflict");
                    }
                    break;
                case BUSCMD_WRITE:
                    ESP_LOGI(mtag, "I2CDRV_BUSCMD_WRITE %d byte(s)", ((i2cdrv_comm_event_data_t *)event_data)->inDataLen);
                    break;
                case BUSCMD_PROBE:
                    ESP_LOGI(mtag, "I2CDRV_BUSPROBE 0x%02X on I2C%u ACK", (((i2cdrv_comm_event_data_t *)event_data)->device_id.i2caddr), (((i2cdrv_comm_event_data_t *)event_data)->device_id.i2cbus));
                    ((i2cdrv_comm_event_data_t *)event_data)->cmd = BUSCMD_RW;
                    ((i2cdrv_comm_event_data_t *)event_data)->type = BUSDATA_UINT8;
                    ((i2cdrv_comm_event_data_t *)event_data)->inDataLen = 1;
                    ((i2cdrv_comm_event_data_t *)event_data)->event_id = 0x0001;                    
                    //*((uint8_t *)(((i2cdrv_comm_event_data_t *)event_data)->InData)) = 0xD0;
                    ((i2cdrv_comm_event_data_t *)event_data)->payload[0] = 0xD0;

                    esp_event_post_to(*uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_OPEXEC, event_data, sizeof(i2cdrv_comm_event_data_t), 1);

                    ((i2cdrv_comm_event_data_t *)event_data)->payload[0] = 0xE1;
                    ((i2cdrv_comm_event_data_t *)event_data)->type = BUSDATA_BLOB;
                    ((i2cdrv_comm_event_data_t *)event_data)->outDataLen = 15;
                    ((i2cdrv_comm_event_data_t *)event_data)->event_id = 0x0010;
                    esp_event_post_to(*uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_OPEXEC, event_data, sizeof(i2cdrv_comm_event_data_t), 1);

                    *((uint8_t *)(((i2cdrv_comm_event_data_t *)event_data)->payload)) = 0x88;
                    ((i2cdrv_comm_event_data_t *)event_data)->outDataLen = 25;
                    ((i2cdrv_comm_event_data_t *)event_data)->event_id = 0x0100;
                    esp_event_post_to(*uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_OPEXEC, event_data, sizeof(i2cdrv_comm_event_data_t), 1);
                default:
            }
        }
    }
    printf("Free memory %d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
}

void app_main(void)
{
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

    /**/

    printf("sizeof(i2cdrv_device_config_t): %d\n", sizeof(i2cdrv_device_config_t));
    uevent_loop = i2cdrv_init();
    esp_event_handler_instance_register_with(*uevent_loop, I2CRESP_EVENT, ESP_EVENT_ANY_ID, main_event_handler, NULL, NULL );

    i2cdrv_comm_event_data_t *data = (i2cdrv_comm_event_data_t *)calloc(1, sizeof(i2cdrv_comm_event_data_t));
    /* BME280 */
    *((i2cdrv_device_config_t *)(data->payload)) = (i2cdrv_device_config_t){
        .dev_config = (i2c_device_config_t) {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = 0x76,
            .scl_speed_hz = 400000 
        },
        .scl_io_num = GPIO_NUM_26,
        .sda_io_num = GPIO_NUM_18    
    };
    
    printf("Free memory %d\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

    esp_event_post_to(*uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_ATTACH, data, sizeof(i2cdrv_comm_event_data_t), 1);

    ((i2cdrv_device_config_t *)data->payload)->scl_io_num = GPIO_NUM_22;
    ((i2cdrv_device_config_t *)data->payload)->sda_io_num = GPIO_NUM_21;

    esp_event_post_to(*uevent_loop, I2CCMND_EVENT, I2CDRV_EVENT_ATTACH, data, sizeof(i2cdrv_comm_event_data_t), 1);

    fflush(stdout);
}
