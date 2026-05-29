idf_component_register(SRCS "sensor-firmware.c"
                            "lib/app_dht/app_dht.c"
                            "lib/mqtt_comms/mqtt_comms.c"
                    INCLUDE_DIRS "." "lib"
                    REQUIRES nvs_flash esp_wifi mqtt cjson dht esp_wifi_manager esp_bus)
