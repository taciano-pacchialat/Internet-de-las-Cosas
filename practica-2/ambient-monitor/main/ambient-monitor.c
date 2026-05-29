#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "cJSON.h"

#include "app_dht/app_dht.h"
#include "mqtt_comms/mqtt_comms.h"
#include "esp_wifi_manager.h"
#include "esp_bus.h"
#define MQTT_BROKER    "mqtt://192.168.1.100"
#define DHT_GPIO       4

static const char *TAG = "MAIN";

#define WIFI_MGR_ENABLE_WEBUI

void sensor_task(void *pvParameters)
{
    float temp = 0.0;
    float hum = 0.0;

    while (1) {
        if (app_dht_read_values(&temp, &hum) == ESP_OK) {
            cJSON *root = cJSON_CreateObject();
            cJSON_AddNumberToObject(root, "temp", temp);
            cJSON_AddNumberToObject(root, "hum", hum);
            
            char *json_str = cJSON_PrintUnformatted(root);
            if (json_str != NULL) {
                ESP_LOGI(TAG, "Publicando: %s", json_str);
                mqtt_comms_publish("sensor/ambiente", json_str);
                free(json_str);
            }
            cJSON_Delete(root);
        } else {
            ESP_LOGE(TAG, "Fallo al leer datos del sensor DHT22");
        }
        
        // Esperar 5 segundos
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    // Inicialización NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Iniciando esp_bus y wifi_manager");
    ESP_ERROR_CHECK(esp_bus_init());

    wifi_manager_config_t config = {
        .default_networks = NULL,
        .default_network_count = 0,
        .default_vars = NULL,
        .default_var_count = 0,
        .max_retry_per_network = 3,
        .retry_interval_ms = 5000,
        .auto_reconnect = true,
        .default_ap = {
            .ssid = "Ambient_Monitor",
            .password = "",
            .channel = 0,
            .max_connections = 4,
            .ip = "192.168.4.1",
            .netmask = "255.255.255.0",
            .gateway = "192.168.4.1",
            .dhcp_start = "192.168.4.2",
            .dhcp_end = "192.168.4.20",
        },
        .enable_captive_portal = true,
        .stop_ap_on_connect = true,
        .http = {
            .enable = true,
            .httpd = NULL,
            .api_base_path = "/api/wifi",
            .enable_auth = false,
        },
    };

    ESP_ERROR_CHECK(wifi_manager_init(&config));

    ESP_LOGI(TAG, "Esperando conexión WiFi...");
    wifi_manager_wait_connected(portMAX_DELAY);
    ESP_LOGI(TAG, "WiFi conectado!");

    ESP_LOGI(TAG, "Inicializando MQTT");
    mqtt_comms_init(MQTT_BROKER);

    ESP_LOGI(TAG, "Inicializando sensor DHT22");
    app_dht_init(DHT_GPIO);

    // Levantar tarea de monitoreo
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
