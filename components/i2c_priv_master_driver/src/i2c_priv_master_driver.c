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

/**
 * @brief Type of slist element for active i2c buses
*/
typedef struct i2cdrv_bus_list {
    i2c_master_bus_handle_t bus_handle; /*!< Master bus handle */
    struct i2cdrv_bus_list *next;       /*!< Pointer to next element */
} i2cdrv_bus_list_t;

typedef struct i2cdrv_device_list {
    uint32_t device_id;
    i2c_master_dev_handle_t dev_handle;
    struct i2cdrv_device_list *next;
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
static esp_err_t i2cdrv_event_post(int32_t event_id, const void *event_data, size_t event_data_size);

/**
 * @brief Find active bus by GPIOs
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

esp_err_t i2cdrv_attach_device(i2cdrv_bus_list_t *bus, i2cdrv_device_config_t *dev_config);

void i2cdrv_deattach_device(uint32_t id);

char *tag ="I2CDRV";

static i2cdrv_internal_config_t *i2cdrv_run_config = NULL;

static void i2cdrv_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if( I2CCMND_EVENT == event_base) {
        if(I2CDRV_EVENT_ATTACH == event_id) {
            i2cdrv_bus_list_t *active_bus = i2cdrv_find_bus( ((i2cdrv_device_config_t *)event_data)->scl_io_num, ((i2cdrv_device_config_t *)event_data)->sda_io_num );
            if(!active_bus) {
                if( !gpio_drv_reserve_pins(BIT64(((i2cdrv_device_config_t *)event_data)->scl_io_num) | BIT64(((i2cdrv_device_config_t *)event_data)->sda_io_num)) ) {
                    i2cdrv_error_event_t error = {
                        .code = DRV_ERR_ATTACH_FAILED,
                        .subcode = DRV_ERR_PIN_ALREADY_USED
                    };
                    i2cdrv_event_post(I2CDRV_EVENT_ERROR, &(error.id), sizeof(uint32_t));
                    return;
                }
                i2c_master_bus_config_t i2c_bus_config = {
                    .clk_source = I2C_CLK_SRC_DEFAULT,
                    .i2c_port = -1,
                    .scl_io_num = ((i2cdrv_device_config_t *)event_data)->scl_io_num,
                    .sda_io_num = ((i2cdrv_device_config_t *)event_data)->sda_io_num,
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
                    /* Error processing */
                    return;
                }
                if(i2cdrv_run_config->i2cdrv_buses) active_bus->next = i2cdrv_run_config->i2cdrv_buses;
                i2cdrv_run_config->i2cdrv_buses = active_bus;
            }
            i2cdrv_attach_device(active_bus, (i2cdrv_device_config_t *)event_data);
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
            i2cdrv_deattach_device(*((uint32_t *)event_data));
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

static esp_err_t i2cdrv_event_post(int32_t event_id, const void *event_data, size_t event_data_size) {
    return (i2cdrv_run_config->i2cdrv_event_loop) ? esp_event_post_to(i2cdrv_run_config->i2cdrv_event_loop, I2CRESP_EVENT, event_id, event_data, event_data_size, 1)
                                  : esp_event_post(I2CRESP_EVENT, event_id, event_data, event_data_size, 1);
}

/* Helper functions */

i2cdrv_bus_list_t *i2cdrv_find_bus(gpio_num_t scl, gpio_num_t sda) {
    i2cdrv_bus_list_t *bus_element = i2cdrv_run_config->i2cdrv_buses;
    //if(!bus_element) return NULL;
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

esp_err_t i2cdrv_attach_device(i2cdrv_bus_list_t *bus, i2cdrv_device_config_t *dev_config) {
    uint32_t new_device_id = ((i2cdrv_device_id_t) {
        .i2caddr = dev_config->dev_config.device_address,
        .i2cbus = bus->bus_handle->base->port_num,
        .gpioscl = dev_config->scl_io_num,
        .gpiosda = dev_config->sda_io_num
    }).id;
    if(i2cdrv_find_device_by_id(new_device_id)) {
        i2cdrv_error_event_t error = {
            .code = DRV_ERR_ATTACH_FAILED,
            .subcode = DRV_ERR_DEVICE_ALREADY_ATTACHED
        };
        i2cdrv_event_post(I2CDRV_EVENT_ERROR, &(error.id), sizeof(uint32_t));
        return ESP_OK;
    }
    i2c_master_dev_handle_t *new_dev_handle = (i2c_master_dev_handle_t *)calloc(1, sizeof(i2c_master_dev_handle_t));
    esp_err_t err = i2c_master_bus_add_device(bus->bus_handle, &(dev_config->dev_config), new_dev_handle);
    if(ESP_OK != err) {
        free(new_dev_handle);
        return ESP_ERR_NO_MEM;
    }
    i2cdrv_device_list_t *new_list_entry = (i2cdrv_device_list_t *)calloc(1, sizeof(i2cdrv_device_list_t));
    if(!new_list_entry) {
        i2c_master_bus_rm_device(*new_dev_handle);
        free(new_dev_handle);
        return ESP_ERR_NO_MEM;
    }
    new_list_entry->dev_handle = *new_dev_handle;
    new_list_entry->device_id = new_device_id;

    if( i2cdrv_run_config->i2c_devices ) new_list_entry->next = i2cdrv_run_config->i2c_devices;
    i2cdrv_run_config->i2c_devices = new_list_entry;
    ESP_LOGI(tag, "Device ID:0x%08lX attached bus %u", new_list_entry->device_id, new_list_entry->dev_handle->master_bus->base->port_num);
    i2cdrv_event_post(I2CDRV_EVENT_ATTACHED, &(new_device_id), sizeof(uint32_t));
    return ESP_OK;
}

void i2cdrv_deattach_device(uint32_t id) {
    i2cdrv_device_list_t *found = i2cdrv_find_device_by_id(id);
    if(!found) {
        i2cdrv_error_event_t error = {
            .code = DRV_ERR_DEATTACH_FAILED,
            .subcode = DRV_ERR_DEVICE_NOT_FOUND
        };
        i2cdrv_event_post(I2CDRV_EVENT_ERROR, &(error.id), sizeof(uint32_t));
    } else {
        i2c_master_bus_handle_t bus = found->dev_handle->master_bus;
        if( ESP_OK == i2c_master_bus_rm_device(found->dev_handle) ) {
            i2cdrv_event_post(I2CDRV_EVENT_DEATTACH, &(found->device_id), sizeof(uint32_t));
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
                if(ESP_OK == i2c_del_master_bus(bus)) {
                    //ESP_LOGI("BUS", "Bus released");
                }
            }
        }
    }
    return;
}

void i2v_priv_drv_dummy(void) {
    return;
}