#include "mqtt_service.h"
#include <cstring>
#include <cstdio>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "cJSON.h"
#include "gateway_config.h"
#include "mdns.h"

static const char* TAG = "MQTT_SVC";

static esp_mqtt_client_handle_t mqtt_client = nullptr;
static volatile bool mqtt_connected = false;
static char s_dynamic_mqtt_uri[128] = {0};

// Buffers para datos recibidos de MQTT (downlink para el edge)
static char downlink_gps_buffer[512] = {0};
static char downlink_fence_buffer[512] = {0};
static volatile bool has_new_gps = false;
static volatile bool has_new_fence = false;

/*
 * =============================================================================
 * RESOLUCIÓN mDNS DINÁMICA DEL BROKER MQTT (Alternativa de Respaldo)
 * =============================================================================
 * Para evitar hardcodeo de IPs en entornos de red cambiantes, consultamos
 * mediante mdns_query_a() al servicio local por el hostname MDNS_TARGET_HOST
 * ("iotbroker"). Si se obtiene un registro A dentro de los 2000 ms, armamos
 * dinámicamente la URI mqtt://<IP_RESUELTA>:1883.
 * Si el lookup expira o falla, recurrimos como fallback a MQTT_BROKER_URI
 * ("mqtt://iotbroker.local:1883").
 * =============================================================================
 */
static const char* resolve_mqtt_uri(void) {
    esp_ip4_addr_t addr;
    ESP_LOGI(TAG, "Consultando mDNS A record para host '%s' (timeout 2000 ms)...", MDNS_TARGET_HOST);
    esp_err_t err = mdns_query_a(MDNS_TARGET_HOST, 2000, &addr);
    if (err == ESP_OK) {
        snprintf(s_dynamic_mqtt_uri, sizeof(s_dynamic_mqtt_uri),
                 "mqtt://" IPSTR ":1883", IP2STR(&addr));
        ESP_LOGI(TAG, "mDNS resuelto exitosamente: %s -> %s", MDNS_TARGET_HOST, s_dynamic_mqtt_uri);
        return s_dynamic_mqtt_uri;
    }
    ESP_LOGW(TAG, "mDNS lookup para '%s' no resolvió IP directa (%s). Usando fallback URI: %s",
             MDNS_TARGET_HOST, esp_err_to_name(err), MQTT_BROKER_URI);
    return MQTT_BROKER_URI;
}

static void mqtt_event_handler(void* arg, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado al broker exitosamente");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SIM_GPS, 1);
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_FENCE_UPDATE, 1);
            esp_mqtt_client_subscribe(mqtt_client, "geofence/config_update", 1);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT datos recibidos en topico: %.*s", event->topic_len, event->topic);

            if (strncmp(event->topic, MQTT_TOPIC_SIM_GPS, event->topic_len) == 0) {
                int copy_len = event->data_len < (int)sizeof(downlink_gps_buffer) - 1
                             ? event->data_len : (int)sizeof(downlink_gps_buffer) - 1;
                memcpy(downlink_gps_buffer, event->data, copy_len);
                downlink_gps_buffer[copy_len] = '\0';
                has_new_gps = true;
                ESP_LOGI(TAG, "GPS simulado recibido: %s", downlink_gps_buffer);
            }
            else if (strncmp(event->topic, MQTT_TOPIC_FENCE_UPDATE, event->topic_len) == 0 ||
                     strncmp(event->topic, "geofence/config_update", event->topic_len) == 0) {
                char temp_buf[1024] = {0};
                int copy_len = event->data_len < (int)sizeof(temp_buf) - 1
                             ? event->data_len : (int)sizeof(temp_buf) - 1;
                memcpy(temp_buf, event->data, copy_len);
                temp_buf[copy_len] = '\0';

                cJSON* root = cJSON_Parse(temp_buf);
                if (root != NULL) {
                    cJSON* min_lat_j = cJSON_GetObjectItem(root, "min_lat");
                    cJSON* max_lat_j = cJSON_GetObjectItem(root, "max_lat");
                    cJSON* min_lon_j = cJSON_GetObjectItem(root, "min_lon");
                    cJSON* max_lon_j = cJSON_GetObjectItem(root, "max_lon");

                    if (cJSON_IsNumber(min_lat_j) && cJSON_IsNumber(max_lat_j) &&
                        cJSON_IsNumber(min_lon_j) && cJSON_IsNumber(max_lon_j)) {
                        snprintf(downlink_fence_buffer, sizeof(downlink_fence_buffer),
                                 "{\"min_lat\":%.6f,\"max_lat\":%.6f,\"min_lon\":%.6f,\"max_lon\":%.6f}",
                                 min_lat_j->valuedouble, max_lat_j->valuedouble,
                                 min_lon_j->valuedouble, max_lon_j->valuedouble);
                        has_new_fence = true;
                        ESP_LOGI(TAG, "Fence update compactado para downlink LoRa (<100 bytes): %s", downlink_fence_buffer);
                    } else {
                        ESP_LOGW(TAG, "Faltan min_lat/max_lat/min_lon/max_lon en JSON de fence recibido");
                    }
                    cJSON_Delete(root);
                } else {
                    ESP_LOGW(TAG, "Fallo al parsear JSON recibido en fence update");
                }
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado del broker");
            mqtt_connected = false;
            break;

        default:
            break;
    }
}

void mqtt_service_init_async(void) {
    if (mqtt_client != nullptr) {
        ESP_LOGI(TAG, "Cliente MQTT ya inicializado");
        return;
    }

    const char* target_uri = resolve_mqtt_uri();

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = target_uri;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    ESP_LOGI(TAG, "Cliente MQTT arrancado en segundo plano hacia: %s", target_uri);
}

void mqtt_init_and_publish(const char* heartbeat_payload) {
    if (mqtt_client == nullptr) {
        mqtt_service_init_async();
    }

    int wait = 0;
    while (!mqtt_connected && wait < 3000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait += 100;
    }

    if (mqtt_connected) {
        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_HEARTBEAT,
                                heartbeat_payload, 0, 1, 0);
        ESP_LOGI(TAG, "Heartbeat publicado: %s", heartbeat_payload);
    } else {
        ESP_LOGW(TAG, "No se pudo publicar (broker MQTT no conectado aún)");
    }
}

bool mqtt_is_ready(void) { return mqtt_connected; }
bool mqtt_has_new_gps(void) { return has_new_gps; }
bool mqtt_has_new_fence(void) { return has_new_fence; }
const char* mqtt_get_downlink_gps(void) { return downlink_gps_buffer; }
const char* mqtt_get_downlink_fence(void) { return downlink_fence_buffer; }
void mqtt_reset_flags(void) { has_new_gps = false; has_new_fence = false; }
