idf_component_register(
    SRCS "src/i2c_priv_master_driver.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_event driver
)
