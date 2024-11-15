/*
 * SPDX-FileCopyrightText: 2024 Rossen Dobrinov
 *
 * SPDX-License-Identifier: Internal use only
 */

/*
 * i2c_private.h
*/

#include "i2c_priv_master_driver.h"
#include "idf_gpio_driver.h"
#include "../i2c_private.h"

#include "esp_log.h"

/**
 * @brief Type of slist element for active i2c buses
*/
typedef struct i2cdrv_bus_list {
    i2c_master_bus_handle_t bus_handle; /*!< Master bus handle */
    struct i2cdrv_bus_list *next;       /*!< Pointer to next element */
} i2cdrv_bus_list_t;

/**
 * @brief Type of slist element for active i2c devices
*/
typedef struct i2cdrv_device_list {
    uint32_t device_id;                 /*!< Internal device ID */
    i2c_master_dev_handle_t dev_handle; /*!< Device handle */
    struct i2cdrv_device_list *next;    /*!< Pionter to next element */
} i2cdrv_device_list_t;

/**
 * @brief Type of driver internal configuration
*/
typedef struct i2cdrv_internal_config {
    union {
        struct {
            uint32_t init_complete:1;   /*!< Init complete */
            uint32_t notused:31;        /*!< Not used */
        };
        uint32_t states_holder;         /*!< Init complete */
    };
    esp_event_loop_handle_t i2cdrv_event_loop;
    i2cdrv_bus_list_t *i2cdrv_buses;
    i2cdrv_device_list_t *i2c_devices;
} i2cdrv_internal_config_t;

ESP_EVENT_DEFINE_BASE(I2CCMND_EVENT);
ESP_EVENT_DEFINE_BASE(I2CRESP_EVENT);

/* Test only */
void hexdump(const uint8_t *buf, size_t len);

/* Driver internal functions */

/**
 * @brief Driver event handler function
 * 
 * @param[in] arg Pointer to event handler args passed at handler registration [null/not used]
 * @param[in] event_base Event base from event loop
 * @param[in] event_id Event id from event loop
 * @param[in] event_data Data from event
 * 
 * @return 
 *      - None
*/
static void i2cdrv_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * @brief Driver event post function
 * 
 * @param[in] event_id Event ID to post
 * @param[in] event_data Pointer to event data to post
 * @param[in] event_data_size Event data size 
 * @return 
 *      - None
*/
static void i2cdrv_event_post(int32_t event_id, const void *event_data, size_t event_data_size);

/**
 * @brief Driver error event post
 * 
 * @param[in] event_id Event ID to post
 * @param[in] event_data Pointer to event data to post

 * @return 
 *      - None
*/
static void i2cdrv_event_err(const void *event_data, i2cdrv_bus_opcodes_t code);

/**
 * @brief Attach device to master bus
 * 
 * @param[bus] Master bus handler to attach device
 * @param[dev_config] I2C device configuration
 * @return 
 *      - ESP_OK if succeed
*/
void i2cdrv_attach_device(i2cdrv_bus_list_t *bus, i2cdrv_comm_event_data_t *event_data);

/**
 * @brief Deattach device to master bus
 * 
 * @param[event_data] Data received with command event
 * @return 
 *      - ESP_OK if succeed
*/
void i2cdrv_deattach_device(i2cdrv_comm_event_data_t *event_data);

/**
 * @brief Find active master bus by GPIOs
 * 
 * @param[in] scl Clock i2c pin
 * @param[in] sda Data i2c pin
 * @return 
 *      - Pointer to internal i2c bus data holder
*/
i2cdrv_bus_list_t *i2cdrv_find_bus(gpio_num_t scl, gpio_num_t sda);

/**
 * @brief Find device by ID
 * 
 * @param[in] id Driver device ID
 * @return 
 *      - Pointer to device ID and Handler
*/
i2cdrv_device_list_t *i2cdrv_find_device_by_id(uint32_t id);

char *tag ="I2CDRV";

static i2cdrv_internal_config_t *i2cdrv_run_config = NULL;

static void i2cdrv_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if( I2CCMND_EVENT == event_base) {
        if(I2CDRV_EVENT_OPEXEC == event_id) {
            ((i2cdrv_comm_event_data_t *)event_data)->code = I2CDRV_BUS_OK;
            /* Find device in device list */
            i2cdrv_device_list_t *device = i2cdrv_find_device_by_id(((i2cdrv_comm_event_data_t *)event_data)->device_id.id);
            if( !device ) {
                i2cdrv_event_err(event_data, I2CDRV_ERR_DEVICE_NOT_FOUND);
                return;
            }
            /* No return value */
            if(I2CDRV_BUSPROBE == ((i2cdrv_comm_event_data_t *)event_data)->cmd) {
                if(ESP_OK != i2c_master_probe(device->dev_handle->master_bus, device->dev_handle->device_address, 1)) {
                    i2cdrv_event_err(event_data, I2CDRV_ERR_DEVICE_NOT_ACK);
                } else {
                    ((i2cdrv_comm_event_data_t *)event_data)->code = I2CDRV_BUS_OK;
                    i2cdrv_event_post(I2CDRV_EVENT_DATA, event_data, sizeof(i2cdrv_comm_event_data_t));
                }
                return;
            }
            ESP_LOGW("dev_handle", "%p, %X", device->dev_handle, ((i2cdrv_comm_event_data_t *)event_data)->cmd);
            /* Write command with empty pointer or zero length */
            if( ((((i2cdrv_comm_event_data_t *)event_data)->cmd == I2CDRV_BUSCMD_WRITE) || (((i2cdrv_comm_event_data_t *)event_data)->cmd == I2CDRV_BUSCMD_RW)) 
                    && (!((i2cdrv_comm_event_data_t *)event_data)->inDataLen || !((i2cdrv_comm_event_data_t *)event_data)->ptrInData)) {
                i2cdrv_event_err(event_data, I2CDRV_BUS_ERR_BAD_ARGS);
                return;
            }
            /* Init pointers by command */
            if((((i2cdrv_comm_event_data_t *)event_data)->cmd == I2CDRV_BUSCMD_READ) || (((i2cdrv_comm_event_data_t *)event_data)->cmd == I2CDRV_BUSCMD_RW)) {
                if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_BLOB && !(((i2cdrv_comm_event_data_t *)event_data)->outDataLen)) {
                    i2cdrv_event_err(event_data, I2CDRV_BUS_ERR_BAD_ARGS);
                } else {
                    switch (((i2cdrv_comm_event_data_t *)event_data)->type) {
                        case I2CDRV_BUSDATA_UINT8:
                            ((i2cdrv_comm_event_data_t *)event_data)->outDataLen = sizeof(uint8_t);
                            break;
                        case I2CDRV_BUSDATA_UINT16:
                            ((i2cdrv_comm_event_data_t *)event_data)->outDataLen = sizeof(uint16_t);
                            break;
                        case I2CDRV_BUSDATA_UINT32:
                            ((i2cdrv_comm_event_data_t *)event_data)->outDataLen = sizeof(uint32_t);
                            break;
                        case I2CDRV_BUSDATA_UINT64:
                            ((i2cdrv_comm_event_data_t *)event_data)->outDataLen = sizeof(uint64_t);
                            break;
                    }
                }
                if(!((i2cdrv_comm_event_data_t *)event_data)->ptrOutData) {
                    ((i2cdrv_comm_event_data_t *)event_data)->ptrOutData = (uint8_t *)calloc(((i2cdrv_comm_event_data_t *)event_data)->inDataLen, sizeof(uint8_t));
                    if(!((i2cdrv_comm_event_data_t *)event_data)->ptrOutData) {
                        i2cdrv_event_err(event_data, I2CDRV_ERR_NO_MEM);
                        return;
                    }
                }
            }
            esp_err_t err = ESP_OK;
            uint8_t test8 = 0x55;
            uint16_t test16 = 0x5AA5;
            uint32_t test32 = 0xA5AA5CAUL;
            uint64_t test64 = 0x2132A5AA5CA3221ULL;
            uint8_t *testblob = (uint8_t *)("1234567890ABDCEF");
            switch (((i2cdrv_comm_event_data_t *)event_data)->cmd) {
                case I2CDRV_BUSCMD_WRITE:
                    ESP_LOGW("I2CDRV_BUSCMD_WRITE","");
                    hexdump(((i2cdrv_comm_event_data_t *)event_data)->ptrInData, ((i2cdrv_comm_event_data_t *)event_data)->inDataLen);
                    //err = i2c_master_transmit(device->dev_handle, ((i2cdrv_comm_event_data_t *)event_data)->ptrInData, ((i2cdrv_comm_event_data_t *)event_data)->inDataLen, 1);
                    break;
                case I2CDRV_BUSCMD_READ:
                    ESP_LOGW("I2CDRV_BUSCMD_READ","");
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_BLOB) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, testblob, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT8) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test8, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT16) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test16, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT32) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test32, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT64) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test64, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    hexdump(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    //err = i2c_master_receive(device->dev_handle, ((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen, 1);
                    break;
                case I2CDRV_BUSCMD_RW:
                    ESP_LOGW("I2CDRV_BUSCMD_RW","");
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_BLOB) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, testblob, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT8) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test8, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT16) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test16, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT32) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test32, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    if(((i2cdrv_comm_event_data_t *)event_data)->type == I2CDRV_BUSDATA_UINT64) memcpy(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, &test64, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    hexdump(((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen);
                    hexdump(((i2cdrv_comm_event_data_t *)event_data)->ptrInData, ((i2cdrv_comm_event_data_t *)event_data)->inDataLen);
                    //err = i2c_master_transmit_receive(device->dev_handle, ((i2cdrv_comm_event_data_t *)event_data)->ptrInData, ((i2cdrv_comm_event_data_t *)event_data)->inDataLen,
                    //                                 ((i2cdrv_comm_event_data_t *)event_data)->ptrOutData, ((i2cdrv_comm_event_data_t *)event_data)->outDataLen, 1);
                    break;
                default:
                    i2cdrv_event_err(event_data, I2CDRV_BUS_ERR_UNKNOWN);
                    return;
            }
            if(ESP_OK != err) i2cdrv_event_err(event_data, (ESP_ERR_TIMEOUT == err ) ? I2CDRV_BUS_ERR_TIMEOUT : I2CDRV_BUS_ERR_UNKNOWN);
            else {
                ((i2cdrv_comm_event_data_t *)event_data)->code = I2CDRV_BUS_OK;
                i2cdrv_event_post(I2CDRV_EVENT_DATA, event_data, sizeof(i2cdrv_comm_event_data_t));
            }
            return;
        }
        if(I2CDRV_EVENT_ATTACH == event_id) {
            uint64_t pinmask = (BIT64(((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->scl_io_num) | 
                        BIT64(((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->sda_io_num));
            i2cdrv_bus_list_t *active_bus = i2cdrv_find_bus( 
                ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->scl_io_num, 
                ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->sda_io_num 
            );
            if(!active_bus) {
                if( !gpio_drv_reserve_pins(pinmask) ) {
                    i2cdrv_event_err(event_data, I2CDRV_ERR_PIN_IN_USE);
                    return;
                }
                i2c_master_bus_config_t i2c_bus_config = {
                    .clk_source = I2C_CLK_SRC_DEFAULT,
                    .i2c_port = -1,
                    .scl_io_num = ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->scl_io_num,
                    .sda_io_num = ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->sda_io_num, 
                    .glitch_ignore_cnt = 7,
                    .flags.enable_internal_pullup = true,
                };
                active_bus = (i2cdrv_bus_list_t *)calloc(1, sizeof(i2cdrv_bus_list_t));
                if(!active_bus) {
                    /* Error processing */ /*No mem*/
                    return;
                }
                esp_err_t err = i2c_new_master_bus(&i2c_bus_config, &(active_bus->bus_handle));
                if( ESP_OK != err ) {
                    free(active_bus);
                    gpio_drv_free_pins(pinmask);
                    i2cdrv_event_err(event_data, (ESP_ERR_NOT_FOUND == err) ? I2CDRV_ERR_NO_MORE_BUSES : I2CDRV_BUS_ERR_UNKNOWN);
                    return;
                }
                if(i2cdrv_run_config->i2cdrv_buses) active_bus->next = i2cdrv_run_config->i2cdrv_buses;
                i2cdrv_run_config->i2cdrv_buses = active_bus;
            }
            i2cdrv_attach_device(active_bus, (((i2cdrv_comm_event_data_t *)event_data)));
            return;
        }

        if(I2CDRV_EVENT_DUMP == event_id) {
            for(i2cdrv_bus_list_t *buses = i2cdrv_run_config->i2cdrv_buses; buses; buses = buses->next) {
                ESP_LOGW("", "I2C%u %p SCL_GPIO%02d SDA_GPIO%02d %p", buses->bus_handle->base->port_num, buses, buses->bus_handle->base->scl_num, buses->bus_handle->base->sda_num, buses->next);
                for( i2c_master_device_list_t *device = buses->bus_handle->device_list.slh_first; device; device = device->next.sle_next) {
                    ESP_LOGW("", "0x%2X %p %p %p", device->device->device_address, device->device, device, device->next.sle_next);
                }
            }
            for(i2cdrv_device_list_t *dev = i2cdrv_run_config->i2c_devices; dev; dev = dev->next) {
                ESP_LOGW("", "%p %08lx %p %p", dev->dev_handle, dev->device_id, dev, dev->next);
            }
        }

        if(I2CDRV_EVENT_DEATTACH == event_id) {
            i2cdrv_deattach_device((i2cdrv_comm_event_data_t *)event_data);
        }
    }
    return;
}

void i2cdrv_init(esp_event_loop_handle_t *uevent_loop) {
    gpio_drv_init();
    /* Create internal structures */
    if(!i2cdrv_run_config) {
        i2cdrv_run_config = (i2cdrv_internal_config_t *)calloc(1, sizeof(i2cdrv_internal_config_t));
        if(i2cdrv_run_config) {
            i2cdrv_run_config->i2cdrv_event_loop = (uevent_loop) ? *uevent_loop : NULL;
            if(i2cdrv_run_config->i2cdrv_event_loop) {
                esp_event_handler_instance_register_with(i2cdrv_run_config->i2cdrv_event_loop, I2CCMND_EVENT, ESP_EVENT_ANY_ID, i2cdrv_event_handler, NULL, NULL );
            } else {
                esp_event_handler_instance_register(I2CCMND_EVENT, ESP_EVENT_ANY_ID, i2cdrv_event_handler, NULL, NULL );
            }
            i2cdrv_run_config->init_complete = true;
        } else {
            ESP_LOGE(tag, "NO_MEM");
        }
    }
}

static void i2cdrv_event_post(int32_t event_id, const void *event_data, size_t event_data_size) {
    (i2cdrv_run_config->i2cdrv_event_loop) ? esp_event_post_to(i2cdrv_run_config->i2cdrv_event_loop, I2CRESP_EVENT, event_id, event_data, event_data_size, 1)
                                  : esp_event_post(I2CRESP_EVENT, event_id, event_data, event_data_size, 1);
}

static void i2cdrv_event_err(const void *event_data, i2cdrv_bus_opcodes_t code) {
    ((i2cdrv_comm_event_data_t *)event_data)->code = code;
    i2cdrv_event_post(I2CDRV_EVENT_ERROR, event_data, sizeof(i2cdrv_comm_event_data_t));
    return;
}

/* Helper functions */

i2cdrv_bus_list_t *i2cdrv_find_bus(gpio_num_t scl, gpio_num_t sda) {
    i2cdrv_bus_list_t *bus_element = i2cdrv_run_config->i2cdrv_buses;
    bool found = false;
    while(bus_element && !found) {
        if( bus_element->bus_handle->base->scl_num == scl && bus_element->bus_handle->base->sda_num == sda ) found = true;
        else bus_element = bus_element->next;
    }
    return bus_element;
}

i2cdrv_device_list_t *i2cdrv_find_device_by_id(uint32_t id) {
    i2cdrv_device_list_t *found = NULL;
    for(i2cdrv_device_list_t *dev = i2cdrv_run_config->i2c_devices; dev && !found; dev = dev->next) {
        if(dev->device_id == id) found = dev;
    }
    return found;
}

void i2cdrv_attach_device(i2cdrv_bus_list_t *bus, i2cdrv_comm_event_data_t *event_data) {
    event_data->device_id = ((i2cdrv_device_id_t) {
        .i2caddr = ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->dev_config.device_address,
        .i2cbus = bus->bus_handle->base->port_num,
        .gpioscl = ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->scl_io_num,
        .gpiosda = ((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->sda_io_num
    });
    if(i2cdrv_find_device_by_id(event_data->device_id.id)) {
        i2cdrv_event_err(event_data, I2CDRV_ERR_DEVICE_ALREADY_ATTACHED);
        return;
    }
    i2c_master_dev_handle_t *new_dev_handle = (i2c_master_dev_handle_t *)calloc(1, sizeof(i2c_master_dev_handle_t));
    esp_err_t err = i2c_master_bus_add_device(bus->bus_handle, &(((i2cdrv_device_config_t *)(((i2cdrv_comm_event_data_t *)event_data)->ptrInData))->dev_config), new_dev_handle);
    if(ESP_OK != err) {
        free(new_dev_handle);
        i2cdrv_event_err(event_data, I2CDRV_ERR_NO_MEM);
    }
    i2cdrv_device_list_t *new_list_entry = (i2cdrv_device_list_t *)calloc(1, sizeof(i2cdrv_device_list_t));
    if(!new_list_entry) {
        i2c_master_bus_rm_device(*new_dev_handle);
        free(new_dev_handle);
        i2cdrv_event_err(event_data, I2CDRV_ERR_NO_MEM);
        return;
    }
    new_list_entry->dev_handle = *new_dev_handle;
    new_list_entry->device_id = event_data->device_id.id;

    if( i2cdrv_run_config->i2c_devices ) new_list_entry->next = i2cdrv_run_config->i2c_devices;
    i2cdrv_run_config->i2c_devices = new_list_entry;
    event_data->code = I2CDRV_BUS_OK;
    i2cdrv_event_post(I2CDRV_EVENT_ATTACHED, event_data, sizeof(i2cdrv_comm_event_data_t));
    return;
}

void i2cdrv_deattach_device(i2cdrv_comm_event_data_t *event_data) {
    i2cdrv_device_list_t *found = i2cdrv_find_device_by_id(event_data->device_id.id);
    if(!found) {
        i2cdrv_event_err(event_data, I2CDRV_ERR_DEVICE_NOT_FOUND);
    } else {
        i2c_master_bus_handle_t bus = found->dev_handle->master_bus;
        if( ESP_OK == i2c_master_bus_rm_device(found->dev_handle) ) {
            event_data->code = I2CDRV_BUS_OK;
            i2cdrv_event_post(I2CDRV_EVENT_DEATTACHED, event_data, sizeof(i2cdrv_comm_event_data_t));
            if( found == i2cdrv_run_config->i2c_devices ) i2cdrv_run_config->i2c_devices = found->next;
            else {
                i2cdrv_device_list_t *last_device = i2cdrv_run_config->i2c_devices;
                while( last_device && (last_device->next != found) ) last_device = last_device->next;
                if(last_device) {
                    last_device->next = found->next;
                    free(found);
                }
            }
        }
        if(!(bus->device_list.slh_first)) {
            i2cdrv_bus_list_t *last_bus = i2cdrv_run_config->i2cdrv_buses;
            if(last_bus->bus_handle == bus) i2cdrv_run_config->i2cdrv_buses = last_bus->next;
            else while( last_bus && (last_bus->next->bus_handle != bus) ) last_bus = last_bus->next;
            if(last_bus) {
                last_bus->next = last_bus->next->next;
                gpio_drv_free_pins( BIT64(bus->base->scl_num) | BIT64(bus->base->sda_num));
                if(ESP_OK != i2c_del_master_bus(bus)) {
                    /* ERROR STATE: Bus not released but internal list is cleared */
                } // else ESP_LOGI("BUS", "Bus released");
            }
        }
    }
    return;
}

/* Test only */

void hexdump(const uint8_t *buf, size_t len) {
    if( !len ) return;
    ESP_LOGI("hexdump", "%p", buf);
    for(int i=0; i<len; i++) printf("%02X ", buf[i]);
    printf("\n");
    return;
}

void i2v_priv_drv_dummy(void) {
    return;
}