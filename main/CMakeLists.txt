idf_component_register(
    SRCS "hello_world_main.c"
    PRIV_REQUIRES spi_flash i2c_priv_master_driver 
    REQUIRES esp_event driver
    INCLUDE_DIRS "."
)
