/*
 * EDGE MODULE — ESP32 DevKit V1 (Collar Ganadero)
 * ==============================================
 * Sistema de Geofencing Ganadero con LoRa (Modularizado)
 */

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "edge_config.h"
#include "rtc_state.h"
#include "geofence.h"
#include "lora_comms.h"

#define CONSOLE_UART        UART_NUM_0
#define CONSOLE_TIMEOUT_MS  15000   // Ventana de input (ms)
#define STEP_NORMAL         0.0010  // ~111m por tecla
#define STEP_FAST           0.0100  // ~1111m por tecla (modo rapido)

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
// CONSOLA UART — Navegación GPS con teclas de flecha
// =============================================================================

// Imprime una línea de estado que se sobreescribe con \r
static void _gps_redraw(double lat, double lon, double step, int secs) {
    double mn_lat, mx_lat, mn_lon, mx_lon;
    rtc_state_get_fence(&mn_lat, &mx_lat, &mn_lon, &mx_lon);
    bool inside = (lat >= mn_lat && lat <= mx_lat &&
                   lon >= mn_lon && lon <= mx_lon);
    printf("\r  Lat:%+.4f Lon:%+.4f | %s | paso:%.4f | %ds | flechas=mover F=rapido ENTER=ok Q=cancel   ",
           lat, lon,
           inside ? "DENTRO" : "FUERA ",
           step, secs);
    fflush(stdout);
}

static void console_update_position(void) {
    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate  = 115200;
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_param_config(CONSOLE_UART, &uart_cfg);
    uart_driver_install(CONSOLE_UART, 256, 0, 0, NULL, 0);

    double lat  = rtc_state_get_lat();
    double lon  = rtc_state_get_lon();
    double step = STEP_NORMAL;
    bool confirm = false;

    // Cabecera fija
    printf("\n\r============================================\n");
    printf("\r   CONTROL DE POSICION GPS — Wake #%lu\n",
           (unsigned long)rtc_state_get_wake_count());
    printf("\r============================================\n");
    printf("\r  (usa las flechas para mover, ENTER para confirmar)\n");
    fflush(stdout);

    int64_t start_us  = esp_timer_get_time();
    int64_t timeout_us = (int64_t)CONSOLE_TIMEOUT_MS * 1000LL;
    int last_secs = -1;

    while (true) {
        int64_t elapsed = esp_timer_get_time() - start_us;
        if (elapsed >= timeout_us) break;

        int secs_left = (int)((timeout_us - elapsed) / 1000000LL);
        if (secs_left != last_secs) {
            last_secs = secs_left;
            _gps_redraw(lat, lon, step, secs_left);
        }

        uint8_t ch = 0;
        if (uart_read_bytes(CONSOLE_UART, &ch, 1, pdMS_TO_TICKS(100)) <= 0)
            continue;

        if (ch == '\r' || ch == '\n') {
            confirm = true;
            break;
        }
        if (ch == 'q' || ch == 'Q') {
            // Cancelar: restaurar posición original
            lat = rtc_state_get_lat();
            lon = rtc_state_get_lon();
            break;
        }
        if (ch == 'f' || ch == 'F') {
            step = (step > STEP_NORMAL) ? STEP_NORMAL : STEP_FAST;
            _gps_redraw(lat, lon, step, last_secs);
            continue;
        }
        if (ch == 0x1B) {
            // Secuencia de escape ANSI: ESC [ A/B/C/D
            uint8_t seq[2] = {0, 0};
            uart_read_bytes(CONSOLE_UART, &seq[0], 1, pdMS_TO_TICKS(50));
            uart_read_bytes(CONSOLE_UART, &seq[1], 1, pdMS_TO_TICKS(50));
            if (seq[0] == '[') {
                switch (seq[1]) {
                    case 'A': lat += step; break;  // ↑ Norte
                    case 'B': lat -= step; break;  // ↓ Sur
                    case 'C': lon += step; break;  // → Este
                    case 'D': lon -= step; break;  // ← Oeste
                }
                _gps_redraw(lat, lon, step, last_secs);
            }
        }
    }

    uart_driver_delete(CONSOLE_UART);

    if (confirm) {
        rtc_state_set_pos(lat, lon);
        printf("\r  OK: posicion confirmada lat=%.6f lon=%.6f\n\n", lat, lon);
    } else {
        printf("\r  --> Sin cambios: lat=%.6f lon=%.6f\n\n",
               rtc_state_get_lat(), rtc_state_get_lon());
    }
    fflush(stdout);
}

// =============================================================================
// PUNTO DE ENTRADA
// =============================================================================

extern "C" void app_main(void) {
    rtc_state_increment_wake_count();
    rtc_state_init_or_log();

    // 0. Ventana de consola para actualizar posicion GPS
    console_update_position();

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
