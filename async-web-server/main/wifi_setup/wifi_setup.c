#include "wifi_setup.h"
#include <esp_log.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <esp_wifi_manager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <arpa/inet.h>

static const char *TAG = "WIFI_SETUP";

static bool wifi_connected = false;
static esp_ip4_addr_t ip_addr = {0};

// Manejador de eventos WiFi
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi iniciado");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi conectado");
                wifi_connected = true;
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi desconectado");
                wifi_connected = false;
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ip_addr = event->ip_info.ip;
            char ip_str[16];
            inet_ntoa_r(ip_addr, ip_str, sizeof(ip_str));
            ESP_LOGI(TAG, "IP asignada: %s", ip_str);
            wifi_connected = true;
        }
    }
}

// Inicializar WiFi (esp_wifi_manager maneja todo)
esp_err_t wifi_setup_init(void) {
    ESP_LOGI(TAG, "Inicializando WiFi");
    
    // Registrar manejador de eventos antes de que wifi_manager inicie
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                               &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                               &wifi_event_handler, NULL);
    
    // wifi_manager ya está inicializado en async-web-server.c
    // Solo esperamos conexión
    int attempts = 0;
    while (!wifi_connected && attempts < 60) {
        vTaskDelay(pdMS_TO_TICKS(500));
        attempts++;
        if (attempts % 10 == 0) {
            ESP_LOGD(TAG, "Esperando WiFi... (%d/60)", attempts);
        }
    }
    
    if (wifi_connected) {
        ESP_LOGI(TAG, "Conexión WiFi exitosa");
        return ESP_OK;
    } else {
        ESP_LOGW(TAG, "Timeout esperando WiFi");
        return ESP_ERR_TIMEOUT;
    }
}

// Verificar conexión
bool wifi_setup_is_connected(void) {
    return wifi_connected;
}

// Obtener IP
esp_ip4_addr_t wifi_setup_get_ip(void) {
    return ip_addr;
}
