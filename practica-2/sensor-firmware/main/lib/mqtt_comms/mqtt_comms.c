#include "mqtt_comms.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "MQTT_COMMS";
static esp_mqtt_client_handle_t client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Conectado al broker MQTT");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Desconectado del broker MQTT");
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "Mensaje publicado exitosamente, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "Error en MQTT");
            break;
        default:
            ESP_LOGD(TAG, "Evento MQTT no manejado, id:%d", event->event_id);
            break;
    }
}

esp_err_t mqtt_comms_init(const char *broker_url) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_url,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client == NULL) {
        ESP_LOGE(TAG, "Fallo al inicializar cliente MQTT");
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Fallo al iniciar cliente MQTT: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t mqtt_comms_publish(const char *topic, const char *payload) {
    if (client == NULL) {
        ESP_LOGE(TAG, "Cliente MQTT no inicializado");
        return ESP_FAIL;
    }
    int msg_id = esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Error publicando mensaje");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Publicación enviada, msg_id=%d", msg_id);
    return ESP_OK;
}
