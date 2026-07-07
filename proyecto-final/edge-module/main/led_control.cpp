#include "led_control.h"
#include "edge_config.h"
#include "driver/rtc_io.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char* TAG = "LEDS";

/*
 * FLUJO CRÍTICO PARA RETENCIÓN DE LEDS:
 *
 * 1. rtc_gpio_init(pin)          → Inicializar como RTC GPIO
 * 2. rtc_gpio_set_direction(OUT) → Configurar como salida
 * 3. rtc_gpio_hold_dis(pin)      → SOLTAR el hold anterior para poder escribir
 * 4. rtc_gpio_set_level(pin, X)  → Escribir nuevo estado
 * 5. rtc_gpio_hold_en(pin)       → RE-ACTIVAR hold (mantiene estado en deep sleep)
 *
 * SIN rtc_gpio_hold_en(), los GPIOs se resetean al entrar en deep sleep
 * y los LEDs se apagan.
 */
void leds_init(void) {
    rtc_gpio_init((gpio_num_t)LED_INSIDE_PIN);
    rtc_gpio_set_direction((gpio_num_t)LED_INSIDE_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);

    rtc_gpio_init((gpio_num_t)LED_OUTSIDE_PIN);
    rtc_gpio_set_direction((gpio_num_t)LED_OUTSIDE_PIN, RTC_GPIO_MODE_OUTPUT_ONLY);
}

void leds_set_geofence_state(bool inside) {
    // Soltar hold previo para poder escribir nuevos valores
    rtc_gpio_hold_dis((gpio_num_t)LED_INSIDE_PIN);
    rtc_gpio_hold_dis((gpio_num_t)LED_OUTSIDE_PIN);

    if (inside) {
        // DENTRO del geofence: Verde ON, Rojo OFF
        rtc_gpio_set_level((gpio_num_t)LED_INSIDE_PIN, 1);
        rtc_gpio_set_level((gpio_num_t)LED_OUTSIDE_PIN, 0);
        ESP_LOGI(TAG, "🟢 Estado: DENTRO del geofence");
    } else {
        // FUERA del geofence: Verde OFF, Rojo ON
        rtc_gpio_set_level((gpio_num_t)LED_INSIDE_PIN, 0);
        rtc_gpio_set_level((gpio_num_t)LED_OUTSIDE_PIN, 1);
        ESP_LOGW(TAG, "🔴 Estado: FUERA del geofence ¡ALERTA!");
    }

    // CRÍTICO: Activar hold para que el estado se mantenga durante deep sleep
    rtc_gpio_hold_en((gpio_num_t)LED_INSIDE_PIN);
    rtc_gpio_hold_en((gpio_num_t)LED_OUTSIDE_PIN);
}
