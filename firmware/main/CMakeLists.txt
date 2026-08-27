idf_component_register(
    SRCS
        "app_main.cc"
        "audio_engine.cc"
        "board_diagnostics.cc"
        "device_config.cc"
        "display_test.cc"
        "m3_state.cc"
        "voice_transport.cc"
        "wifi_manager.cc"
    INCLUDE_DIRS "."
    REQUIRES
        driver
        esp_app_format
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_i2s
        esp_driver_spi
        esp_event
        esp_lcd
        esp_netif
        esp_psram
        esp_partition
        esp_wifi
        esp_codec_dev
        esp_websocket_client
        mbedtls
        nvs_flash
        spi_flash
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers)
