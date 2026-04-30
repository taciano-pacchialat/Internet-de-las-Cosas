#include "esp_spiffs.h"
#include "esp_bus.h"
#include "esp_wifi_manager.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "esp_log.h"
#include "web_server.h"
#include "dht11.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <arpa/inet.h>
#include <string.h>

static const char *TAG = "MAIN";

static void on_wifi_event(const char *event, const void *data, size_t len, void *ctx)
{
    (void)data;
    (void)len;
    (void)ctx;

    if (strcmp(event, WIFI_EVT(WIFI_MGR_EVT_AP_STOP)) == 0) {
        ESP_LOGI(TAG, "AP stopped, closing stale WebSocket clients");
        web_server_close_websocket_clients();
    }
}

esp_vfs_spiffs_conf_t conf = {
    .base_path = "/spiffs",
    .partition_label = "spiffs",
    .max_files = 5,
    .format_if_mount_failed = true
};

void app_main(void)
{
    // Inicializar NVS
    ESP_LOGI(TAG, "Inicializando NVS");
    
    esp_err_t ret = nvs_flash_init();
    // nvs_flash_erase();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS necesita recuperacion, borrando una vez");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Inicializar SPIFFS
    ESP_LOGI(TAG, "Montando SPIFFS");
    esp_vfs_spiffs_register(&conf);
    
    // Inicializar bus de eventos
    ESP_LOGI(TAG, "Inicializando esp_bus");
    esp_bus_init();

    esp_bus_sub(WIFI_EVT(WIFI_MGR_EVT_AP_STOP), on_wifi_event, NULL);
    
    // Inicializar DHT11
    ESP_LOGI(TAG, "Inicializando DHT11");
    if (dht11_init() != ESP_OK) {
        ESP_LOGE(TAG, "Error en DHT11");
    }
    
    // Iniciar tarea DHT11 (baja prioridad)
    if (dht11_start_task() != ESP_OK) {
        ESP_LOGE(TAG, "Error iniciar tarea DHT11");
    }
    
    // Configuración del wifi manager con portal cautivo
    ESP_LOGI(TAG, "Inicializando WiFi Manager");
    wifi_manager_init(&(wifi_manager_config_t){
        .http = {
            .enable = true,
        },
        .enable_captive_portal = true,
    });
    
    // Inicializar servidor web (debe ser después de wifi_manager_init)
    ESP_LOGI(TAG, "Inicializando servidor web");
    vTaskDelay(pdMS_TO_TICKS(500)); // Esperar a que wifi_manager prepare el servidor
    if (web_server_start() != ESP_OK) {
        ESP_LOGE(TAG, "Error en servidor web");
    }
    
    // Esperar WiFi con timeout
    ESP_LOGI(TAG, "Esperando conexión WiFi");
    if (wifi_manager_wait_connected(30000) == ESP_OK) {
        // WiFi conectado - obtener IP
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
            char ip_str[16];
            inet_ntoa_r(ip_info.ip, ip_str, sizeof(ip_str));
            ESP_LOGI(TAG, "=== IP FIJA: %s ===", ip_str);
        }
    } else {
        ESP_LOGW(TAG, "WiFi no conectado - portal cautivo activo");
    }
    
    // Loop principal
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
