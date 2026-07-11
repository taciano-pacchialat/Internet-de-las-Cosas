/*
 * EDGE MODULE — ESP32-CAM (Collar Ganadero)
 * ==========================================
 * Sistema de Geofencing Ganadero con LoRa (Modularizado)
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "edge_config.h"
#include "rtc_state.h"
#include "geofence.h"
#include "lora_comms.h"

static const char* TAG = "EDGE_MAIN";

// =============================================================================
// DEEP SLEEP — Timer wakeup
// =============================================================================

static void enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Configurando deep sleep por timer: %llu us (%llu seg)",
             SLEEP_DURATION_US, SLEEP_DURATION_US / 1000000ULL);

    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);

    ESP_LOGI(TAG, "Entrando en Deep Sleep... Zzz");
    esp_deep_sleep_start();
}

// =============================================================================
// PUNTO DE ENTRADA
// =============================================================================

extern "C" void app_main(void) {
    rtc_state_increment_wake_count();
    rtc_state_init_or_log();

    // 1. Computar Geofence (Bounding Box)
    double min_lat, max_lat, min_lon, max_lon;
    rtc_state_get_fence(&min_lat, &max_lat, &min_lon, &max_lon);
    bool inside = geofence_is_inside(rtc_state_get_lat(), rtc_state_get_lon(),
                                     min_lat, max_lat, min_lon, max_lon);
    rtc_state_set_last_inside(inside);

    ESP_LOGI(TAG, "Estado Geofence computado: %s", inside ? "DENTRO" : "FUERA");

    // 2. Inicializar LoRa en HSPI y transmitir heartbeat
    if (lora_init()) {
        lora_send_heartbeat(inside);

        // 3. Esperar downlink del Gateway
        lora_wait_for_downlink();

        // Si cambió la posición o el geofence tras el downlink, actualizar el estado
        rtc_state_get_fence(&min_lat, &max_lat, &min_lon, &max_lon);
        bool new_inside = geofence_is_inside(rtc_state_get_lat(), rtc_state_get_lon(),
                                             min_lat, max_lat, min_lon, max_lon);
        if (new_inside != inside) {
            ESP_LOGI(TAG, "Estado de geofence cambió tras downlink: %s", new_inside ? "DENTRO" : "FUERA");
            rtc_state_set_last_inside(new_inside);
        }

        lora_cleanup();
    } else {
        ESP_LOGE(TAG, "LoRa init falló, durmiendo sin transmitir");
    }

    // 4. Deep Sleep
    enter_deep_sleep();
}
