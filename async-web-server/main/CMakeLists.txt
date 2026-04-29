idf_component_register(SRCS "async-web-server.c"
                              "wifi_setup/wifi_setup.c"
                              "web_server/web_server.c"
                              "dht11/dht11.c"
                    INCLUDE_DIRS "." "wifi_setup" "web_server" "dht11"
                    REQUIRES nvs_flash spiffs esp_bus esp_wifi_manager esp_http_server)
