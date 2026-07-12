#include "lora_comms.h"
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"
#include "edge_config.h"
#include "esp_hal.hpp"
#include "rtc_state.h"

static const char* TAG = "LORA";

static EspHal* hal = nullptr;
static SX1278* lora = nullptr;
static Module* mod = nullptr;

bool lora_init(void) {
    hal = new EspHal(LORA_SCK, LORA_MISO, LORA_MOSI);

    hal->spiBegin();
    hal->spiAddDevice(LORA_CS);

    // --- DIAGNÓSTICO SPI ---
    // Reset manual del módulo antes de leer
    hal->pinMode(LORA_RST, OUTPUT);
    hal->digitalWrite(LORA_RST, LOW);
    hal->delay(10);
    hal->digitalWrite(LORA_RST, HIGH);
    hal->delay(10);

    // Leer RegVersion (0x42) directamente — SX1278 debe responder 0x12
    hal->pinMode(LORA_CS, OUTPUT);
    hal->digitalWrite(LORA_CS, HIGH);
    hal->delay(1);
    uint8_t tx[2] = {0x42, 0x00};
    uint8_t rx[2] = {0x00, 0x00};
    hal->digitalWrite(LORA_CS, LOW);
    hal->spiTransfer(tx, 2, rx);
    hal->digitalWrite(LORA_CS, HIGH);
    ESP_LOGI(TAG, "=== DIAG SPI: RegVersion(0x42) = 0x%02X (esperado 0x12) ===", rx[1]);
    // --- FIN DIAGNÓSTICO ---

    mod = new Module(hal, LORA_CS, LORA_DIO0, LORA_RST, RADIOLIB_NC);
    lora = new SX1278(mod);

    ESP_LOGI(TAG, "Inicializando SX1278 en VSPI...");
    int state = lora->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_POWER, LORA_PREAMBLE);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "SX1278 init falló: %d", state);
        return false;
    }
    ESP_LOGI(TAG, "SX1278 OK");
    return true;
}

void lora_send_heartbeat(bool inside) {
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"device_id\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,"
        "\"fence_status\":\"%s\",\"wake_count\":%lu}",
        DEVICE_ID, rtc_state_get_lat(), rtc_state_get_lon(),
        inside ? "INSIDE" : "OUTSIDE",
        (unsigned long)rtc_state_get_wake_count());

    ESP_LOGI(TAG, "LoRa TX: %s", payload);
    int state = lora->transmit(payload);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "LoRa TX falló: %d", state);
    }
}

/*
 * Espera un downlink del Gateway con formato:
 * {"gps":{"lat":-34.XX,"lon":-57.XX,...},"fence":{"min_lat":...,...}}
 *
 * Parsea con cJSON y actualiza las variables RTC.
 */
void lora_wait_for_downlink(void) {
    ESP_LOGI(TAG, "Esperando downlink LoRa (%d ms)...", RX_WAIT_TIMEOUT_MS);

    int state = lora->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "startReceive falló: %d", state);
        return;
    }

    unsigned long start = (unsigned long)(esp_timer_get_time() / 1000);
    bool received = false;

    while (((unsigned long)(esp_timer_get_time() / 1000) - start) < RX_WAIT_TIMEOUT_MS) {
        if (hal->digitalRead(LORA_DIO0) == HIGH) {
            received = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (!received) {
        ESP_LOGI(TAG, "No se recibió downlink (timeout)");
        return;
    }

    char data[256] = {0};
    state = lora->readData((uint8_t*)data, sizeof(data) - 1);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "readData falló: %d", state);
        return;
    }
    size_t len = lora->getPacketLength();
    if (len >= sizeof(data)) len = sizeof(data) - 1;
    data[len] = '\0';

    ESP_LOGI(TAG, "Downlink recibido: %s", data);

    cJSON* root = cJSON_Parse(data);
    if (root == NULL) {
        ESP_LOGW(TAG, "JSON parse falló");
        return;
    }

    // Parsear GPS simulado
    cJSON* gps = cJSON_GetObjectItem(root, "gps");
    if (gps != NULL && !cJSON_IsNull(gps)) {
        cJSON* lat_j = cJSON_GetObjectItem(gps, "lat");
        cJSON* lon_j = cJSON_GetObjectItem(gps, "lon");

        if (cJSON_IsNumber(lat_j) && cJSON_IsNumber(lon_j)) {
            rtc_state_set_pos(lat_j->valuedouble, lon_j->valuedouble);
            ESP_LOGI(TAG, "GPS actualizado: lat=%.6f, lon=%.6f", lat_j->valuedouble, lon_j->valuedouble);
        }
    }

    // Parsear límites del geofence
    cJSON* fence = cJSON_GetObjectItem(root, "fence");
    if (fence != NULL && !cJSON_IsNull(fence)) {
        cJSON* min_lat_j = cJSON_GetObjectItem(fence, "min_lat");
        cJSON* max_lat_j = cJSON_GetObjectItem(fence, "max_lat");
        cJSON* min_lon_j = cJSON_GetObjectItem(fence, "min_lon");
        cJSON* max_lon_j = cJSON_GetObjectItem(fence, "max_lon");

        if (cJSON_IsNumber(min_lat_j) && cJSON_IsNumber(max_lat_j) &&
            cJSON_IsNumber(min_lon_j) && cJSON_IsNumber(max_lon_j)) {
            rtc_state_set_fence(min_lat_j->valuedouble, max_lat_j->valuedouble,
                                min_lon_j->valuedouble, max_lon_j->valuedouble);
            ESP_LOGI(TAG, "Geofence actualizado: [%.6f,%.6f] x [%.6f,%.6f]",
                     min_lat_j->valuedouble, max_lat_j->valuedouble,
                     min_lon_j->valuedouble, max_lon_j->valuedouble);
        }
    }

    cJSON_Delete(root);
}

void lora_cleanup(void) {
    if (lora) { delete lora; lora = nullptr; }
    if (mod)  { delete mod;  mod = nullptr;  }
    if (hal)  { hal->spiEnd(); delete hal; hal = nullptr; }
}
