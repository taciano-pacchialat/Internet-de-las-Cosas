/*
 * GATEWAY MODULE — ESP32 Clásico (Modularizado)
 * ==============================================
 * Sistema de Geofencing Ganadero con LoRa
 */

#include <cstdio>
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gateway_config.h"
#include "wifi_service.h"
#include "mqtt_service.h"
#include "lora_service.h"

static const char* TAG = "GW_MAIN";
static TaskHandle_t s_lora_rx_task_handle = NULL;

// =============================================================================
// INTERRUPCIÓN LORA (ISR en IRAM) Y TAREA DE PROCESAMIENTO (Smell #2)
// =============================================================================

static void IRAM_ATTR lora_dio0_isr_handler(void* arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_lora_rx_task_handle != NULL) {
        vTaskNotifyGiveFromISR(s_lora_rx_task_handle, &xHigherPriorityTaskWoken);
    }
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

static void lora_rx_processing_task(void* arg) {
    char rx_buffer[256] = {0};
    float rssi = 0.0f, snr = 0.0f;

    while (true) {
        // Bloquear tarea esperando notificación de la ISR (consumo CPU = 0% en espera)
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        ESP_LOGI(TAG, "Notificación ISR (DIO0) recibida. Leyendo paquete LoRa por SPI...");
        if (lora_receive(rx_buffer, sizeof(rx_buffer), &rssi, &snr)) {
            if (wifi_init_and_connect()) {
                char mqtt_payload[768];
                snprintf(mqtt_payload, sizeof(mqtt_payload),
                    "{\"raw\":%s,\"rssi\":%.1f,\"snr\":%.1f}",
                    rx_buffer, rssi, snr);

                mqtt_init_and_publish(mqtt_payload);

                if (mqtt_has_new_gps() || mqtt_has_new_fence()) {
                    char downlink[768];
                    snprintf(downlink, sizeof(downlink),
                        "{\"gps\":%s,\"fence\":%s}",
                        mqtt_has_new_gps() ? mqtt_get_downlink_gps() : "null",
                        mqtt_has_new_fence() ? mqtt_get_downlink_fence() : "null");

                    lora_send_downlink(downlink);
                    mqtt_reset_flags();
                }

                wifi_disconnect_and_deinit();
            }
        }

        // Volver a poner en modo RX continuo para el próximo paquete
        lora_set_rx_and_prepare_sleep();
    }
}

// =============================================================================
// DEEP SLEEP — Configuración con wakeup por ext0 (DIO0 / GPIO2)
// =============================================================================

static void __attribute__((unused)) enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Configurando deep sleep con wakeup por GPIO%d (DIO0)...", LORA_DIO0);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)LORA_DIO0, 1);  // 1 = HIGH level
    ESP_LOGI(TAG, "Entrando en Deep Sleep... Zzz");
    esp_deep_sleep_start();
}

// =============================================================================
// PUNTO DE ENTRADA (app_main)
// =============================================================================

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== Gateway Module - Iniciando (Modo Demostración en Vivo / Producción) ===");

    uint32_t wakeup_causes = esp_sleep_get_wakeup_causes();
    ESP_LOGI(TAG, "Wakeup causes bitmap: 0x%lx", (unsigned long)wakeup_causes);

    // 1. Inicializar NVS (necesario para Wi-Fi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializar LoRa
    if (!lora_init()) {
        ESP_LOGE(TAG, "Fallo crítico inicializando LoRa, reiniciando...");
        esp_restart();
    }

    // 3. Crear tarea FreeRTOS para procesamiento RX
    xTaskCreate(lora_rx_processing_task, "lora_rx_task", 8192, NULL, 5, &s_lora_rx_task_handle);

    // 4. Configurar interrupción en el pin DIO0 (GPIO26)
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_POSEDGE; // Flanco de subida cuando llega paquete LoRa
    io_conf.pin_bit_mask = (1ULL << LORA_DIO0);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_config(&io_conf);

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "Servicio ISR GPIO ya instalado o error: %d", err);
    }
    gpio_isr_handler_add((gpio_num_t)LORA_DIO0, lora_dio0_isr_handler, NULL);

    // 5. Poner LoRa en modo escucha continua
    lora_set_rx_and_prepare_sleep();

    // 6. Si despertamos por ext0 (llegó un paquete mientras dormía), notificar inmediatamente
    if (wakeup_causes & (1UL << ESP_SLEEP_WAKEUP_EXT0)) {
        ESP_LOGI(TAG, "Despertado por ext0 (DIO0). Disparando tarea FreeRTOS...");
        xTaskNotifyGive(s_lora_rx_task_handle);
    } else {
        ESP_LOGI(TAG, "Boot inicial / Modo continuo activo en FreeRTOS. Esperando paquetes LoRa...");
    }
}
