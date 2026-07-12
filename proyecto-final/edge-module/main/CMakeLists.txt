idf_component_register(
    SRCS "edge-module.cpp"
         "rtc_state.cpp"
         "geofence.cpp"
         "lora_comms.cpp"
    INCLUDE_DIRS "."
    REQUIRES espressif__cjson driver esp_driver_gpio esp_driver_spi esp_driver_uart esp_timer esp_system freertos
)
