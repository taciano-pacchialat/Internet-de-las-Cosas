idf_component_register(SRCS "ambient-monitor.c"
                            "lib/dht/dht.c"
                            "lib/mqtt_comms/mqtt_comms.c"
                    INCLUDE_DIRS "." "lib"
                    REQUIRES nvs_flash esp_wifi mqtt json)
