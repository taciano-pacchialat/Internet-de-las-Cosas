idf_component_register(
    SRCS "edge-module.cpp"
         "rtc_state.cpp"
         "geofence.cpp"
         "led_control.cpp"
         "lora_comms.cpp"
    INCLUDE_DIRS "."
    REQUIRES espressif__cjson driver esp_driver_gpio esp_driver_spi esp_timer esp_system freertos
)
