#include "dht.h"
#include <dht.h> // Incluye el header de la librería uncle-rus/dht
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "APP_DHT";
static int dht_gpio = 4; // GPIO por defecto

esp_err_t app_dht_init(int gpio_num) {
    dht_gpio = gpio_num;
    ESP_LOGI(TAG, "DHT inicializado en GPIO %d", dht_gpio);
    return ESP_OK;
}

esp_err_t app_dht_read_values(float *temp, float *hum) {
    // Lectura del sensor tipo DHT22 / AM2302
    esp_err_t res = dht_read_float_data(DHT_TYPE_DHT22, (gpio_num_t)dht_gpio, hum, temp);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Error al leer sensor DHT: %s", esp_err_to_name(res));
        return res;
    }
    ESP_LOGI(TAG, "Humedad: %.1f%% Temp: %.1fC", *hum, *temp);
    return ESP_OK;
}
