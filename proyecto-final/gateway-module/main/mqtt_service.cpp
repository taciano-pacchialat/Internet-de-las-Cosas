#include "mqtt_service.h"
#include <cstring>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "gateway_config.h"

static const char* TAG = "MQTT";

static esp_mqtt_client_handle_t mqtt_client = nullptr;
static bool mqtt_connected = false;

// Buffers para datos recibidos de MQTT (downlink para el edge)
static char downlink_gps_buffer[256] = {0};
static char downlink_fence_buffer[256] = {0};
static bool has_new_gps = false;
static bool has_new_fence = false;

static void mqtt_event_handler(void* arg, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT conectado");
            mqtt_connected = true;
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_SIM_GPS, 1);
            esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC_FENCE_UPDATE, 1);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT datos recibidos en: %.*s", event->topic_len, event->topic);

            if (strncmp(event->topic, MQTT_TOPIC_SIM_GPS, event->topic_len) == 0) {
                int copy_len = event->data_len < (int)sizeof(downlink_gps_buffer) - 1
                             ? event->data_len : (int)sizeof(downlink_gps_buffer) - 1;
                memcpy(downlink_gps_buffer, event->data, copy_len);
                downlink_gps_buffer[copy_len] = '\0';
                has_new_gps = true;
                ESP_LOGI(TAG, "GPS simulado recibido: %s", downlink_gps_buffer);
            }
            else if (strncmp(event->topic, MQTT_TOPIC_FENCE_UPDATE, event->topic_len) == 0) {
                int copy_len = event->data_len < (int)sizeof(downlink_fence_buffer) - 1
                             ? event->data_len : (int)sizeof(downlink_fence_buffer) - 1;
                memcpy(downlink_fence_buffer, event->data, copy_len);
                downlink_fence_buffer[copy_len] = '\0';
                has_new_fence = true;
                ESP_LOGI(TAG, "Fence update recibido: %s", downlink_fence_buffer);
            }
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado");
            mqtt_connected = false;
            break;

        default:
            break;
    }
}

void mqtt_init_and_publish(const char* heartbeat_payload) {
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URI;

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    int wait = 0;
    while (!mqtt_connected && wait < 5000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait += 100;
    }

    if (mqtt_connected) {
        esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_HEARTBEAT,
                                heartbeat_payload, 0, 1, 0);
        ESP_LOGI(TAG, "Heartbeat publicado: %s", heartbeat_payload);

        ESP_LOGI(TAG, "Esperando actualizaciones MQTT (%d ms)...", MQTT_SUBSCRIBE_WAIT_MS);
        vTaskDelay(pdMS_TO_TICKS(MQTT_SUBSCRIBE_WAIT_MS));
    } else {
        ESP_LOGW(TAG, "No se pudo conectar a MQTT, omitiendo publicación");
    }

    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = nullptr;
    mqtt_connected = false;
}

bool mqtt_has_new_gps(void) { return has_new_gps; }
bool mqtt_has_new_fence(void) { return has_new_fence; }
const char* mqtt_get_downlink_gps(void) { return downlink_gps_buffer; }
const char* mqtt_get_downlink_fence(void) { return downlink_fence_buffer; }
void mqtt_reset_flags(void) { has_new_gps = false; has_new_fence = false; }
