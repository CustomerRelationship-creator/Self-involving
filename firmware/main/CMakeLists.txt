idf_component_register(
    SRCS
        "app_main.cc"
        "board_diagnostics.cc"
        "display_test.cc"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_lcd
        esp_psram
        esp_partition
        nvs_flash
)
