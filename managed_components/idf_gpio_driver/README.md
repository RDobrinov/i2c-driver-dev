# IDF GPIO reservation and pin status component

Simple pin status and reservation component. System reserved GPIO pins and ststus function covers
only the ESP32, ESP32-S3, and ESP32-C6 boards.

![](https://img.shields.io/badge/dynamic/yaml?url=https://raw.githubusercontent.com/RDobrinov/idf_gpio_driver/main/idf_component.yml&query=$.version&style=plastic&color=%230f900f&label)
![](https://img.shields.io/badge/dynamic/yaml?url=https://raw.githubusercontent.com/RDobrinov/idf_gpio_driver/main/idf_component.yml&query=$.dependencies.idf&style=plastic&logo=espressif&label=IDF%20Ver.)
![](https://img.shields.io/badge/-ESP32-rgb(37,194,160)?style=plastic&logo=espressif)
![](https://img.shields.io/badge/-ESP32--S3-rgb(37,194,160)?style=plastic&logo=espressif)
![](https://img.shields.io/badge/-ESP32--C6-rgb(37,194,160)?style=plastic&logo=espressif)

---

## Info

System and unusable gpio can be mark with *__gpio_drv_init()__*. Status functions can be controlled by Kconfig
For all API functions read comments in header file.

## Installation

1. Create *idf_component.yml*
```
idf.py create-manifest
```
2. Edit ***idf_component.yml*** to add dependency
```
dependencies:
  ...
  idf_gpio_driver:
    version: "main"
    git: git@github.com:RDobrinov/idf_gpio_driver.git
  ...
```
3. Reconfigure project

or 

4. Download and unzip component in project ***components*** folder

### Example
```
#include <stdio.h>
#include "idf_gpio_driver.h"

void app_main(void)
{
    gpio_drv_init();
    gpio_drv_reserve(GPIO_NUM_9);
    char *status = gpio_drv_get_io_description(GPIO_NUM_9, false);
    printf("%s", status);
    free(status);
    gpio_drv_free(GPIO_NUM_9);
    status = gpio_drv_get_io_description(GPIO_NUM_9, true);
    printf("%s", status);
    free(status);
}
```
