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

#include "dht/dht.h"
#include "mqtt_comms/mqtt_comms.h"

#define WIFI_SSID      "MI_WIFI"
#define WIFI_PASS      "MI_PASSWORD"
#define MQTT_BROKER    "mqtt://192.168.1.100"
#define DHT_GPIO       4

static const char *TAG = "MAIN";

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Reintentando conectar al AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP obtenida:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void)
{
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();

    ESP_LOGI(TAG, "wifi_init_sta completado.");
}

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

    ESP_LOGI(TAG, "Iniciando modo STA WiFi");
    wifi_init_sta();

    // Esperar unos segundos para que se conecte la red antes de lanzar MQTT
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "Inicializando MQTT");
    mqtt_comms_init(MQTT_BROKER);

    ESP_LOGI(TAG, "Inicializando sensor DHT22");
    app_dht_init(DHT_GPIO);

    // Levantar tarea de monitoreo
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}
