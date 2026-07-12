idf_component_register(
    SRCS "gateway-module.cpp"
         "wifi_service.cpp"
         "mqtt_service.cpp"
         "lora_service.cpp"
    INCLUDE_DIRS "."
    REQUIRES espressif__cjson mqtt esp_wifi nvs_flash esp_event esp_netif driver esp_driver_gpio esp_driver_spi esp_timer esp_system freertos jackhuang021__esp32-wifi-manager mdns
)
