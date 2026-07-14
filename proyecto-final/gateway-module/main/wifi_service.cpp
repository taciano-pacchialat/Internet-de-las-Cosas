#include "wifi_service.h"
#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gateway_config.h"
#include "wifi_manager.h"
#include "mdns.h"
#include "mqtt_service.h"

static const char* TAG = "WIFI_MGR";

// Función definida en gateway-module.cpp — inicializa LoRa post-WiFi
extern "C" void gateway_start_radio(void);

static volatile bool s_wifi_connected = false;
static bool s_mdns_initialized = false;

static void on_wifi_disconnected(void* param) {
    ESP_LOGW(TAG, "Wi-Fi STA desconectado. Reintentando o esperando configuración en SoftAP...");
    s_wifi_connected = false;
}

static void on_wifi_connected(void* param) {
    ESP_LOGI(TAG, "=== Conexión Wi-Fi STA Exitosa (IP Obtenida) ===");
    s_wifi_connected = true;

    // Inicializar servicio mDNS para resolución local
    if (!s_mdns_initialized) {
        esp_err_t err = mdns_init();
        if (err == ESP_OK) {
            mdns_hostname_set(MDNS_HOSTNAME);
            mdns_instance_name_set("ESP32 LoRa Gateway Module");
            mdns_service_add("ESP32 Gateway Service", "_iotgateway", "_tcp", 80, NULL, 0);
            ESP_LOGI(TAG, "Servicio mDNS inicializado. Hostname: %s.local", MDNS_HOSTNAME);
            s_mdns_initialized = true;
        } else {
            ESP_LOGE(TAG, "Fallo al inicializar mDNS: %s", esp_err_to_name(err));
        }
    }

    // Inicializar el cliente MQTT de forma asíncrona tras obtener red local
    ESP_LOGI(TAG, "Iniciando cliente MQTT asíncrono...");
    mqtt_service_init_async();

    // Inicializar subsistema LoRa DESPUÉS de WiFi+MQTT estable
    // para evitar brownout por consumo simultáneo.
    gateway_start_radio();
}

void wifi_service_init(void) {
    ESP_LOGI(TAG, "Configurando e iniciando esp32-wifi-manager...");

    // 1. Configuración del SoftAP para la demostración / provisión visual
    strncpy((char*)wifi_settings.ap_ssid, "Gateway Network Setup", sizeof(wifi_settings.ap_ssid) - 1);
    wifi_settings.ap_ssid[sizeof(wifi_settings.ap_ssid) - 1] = '\0';
    wifi_settings.ap_pwd[0] = '\0'; // Sin contraseña (AP abierto) según requerimiento del Paso 2
    wifi_settings.ap_ssid_hidden = 0;

    // 2. Iniciar tarea y servidor web de esp32-wifi-manager (asigna memoria para callbacks)
    wifi_manager_start();

    // 3. Registrar callbacks de eventos Wi-Fi DESPUÉS de wifi_manager_start()
    wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &on_wifi_connected);
    wifi_manager_set_callback(WM_EVENT_STA_DISCONNECTED, &on_wifi_disconnected);

    ESP_LOGI(TAG, "SoftAP levantado en SSID: 'Gateway Network Setup' (abierto). Servidor web listo en 10.10.0.1");
}

bool wifi_is_connected(void) {
    return s_wifi_connected;
}

void wifi_wait_for_connection(uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (!s_wifi_connected && elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(100));
        elapsed += 100;
    }
}

void wifi_disconnect_and_deinit(void) {
    ESP_LOGI(TAG, "Deteniendo wifi_manager...");
    wifi_manager_destroy();
    s_wifi_connected = false;
}
