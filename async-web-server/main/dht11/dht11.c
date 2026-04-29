#include "dht11.h"
#include "web_server.h"
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <time.h>
#include <dht.h>

#define DHT11_GPIO GPIO_NUM_4
#define DHT11_BUFFER_SIZE 30

static const char *TAG = "DHT11";

// Buffer circular para histórico
typedef struct {
    dht11_reading_t readings[DHT11_BUFFER_SIZE];
    int head;
    int count;
} dht11_buffer_t;

static dht11_buffer_t buffer = {0};
static TaskHandle_t dht11_task_handle = NULL;

// Inicializar DHT11
esp_err_t dht11_init(void) {
    esp_err_t ret = gpio_set_direction(DHT11_GPIO, GPIO_MODE_OUTPUT_OD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configurar GPIO");
        return ret;
    }

    gpio_set_pull_mode(DHT11_GPIO, GPIO_PULLUP_ONLY);
    gpio_set_level(DHT11_GPIO, 1);
    
    buffer.head = 0;
    buffer.count = 0;
    
    ESP_LOGI(TAG, "DHT11 inicializado en GPIO %d", DHT11_GPIO);
    return ESP_OK;
}

// Leer sensor
esp_err_t dht11_read(float *temperature, float *humidity) {
    esp_err_t err = ESP_FAIL;
    
    // Intentar lectura con reintentos
    for (int attempt = 0; attempt < 3; attempt++) {
        err = dht_read_float_data(DHT_TYPE_DHT11, DHT11_GPIO, humidity, temperature);

        if (err == ESP_OK) {
            if (temperature && humidity) {
                ESP_LOGI(TAG, "Lectura OK: T=%.1f°C, H=%.1f%%", *temperature, *humidity);
            } else if (temperature) {
                ESP_LOGI(TAG, "Lectura OK: T=%.1f°C", *temperature);
            } else if (humidity) {
                ESP_LOGI(TAG, "Lectura OK: H=%.1f%%", *humidity);
            } else {
                ESP_LOGI(TAG, "Lectura OK");
            }

            return ESP_OK;
        }
        
        if (attempt < 2) {
            vTaskDelay(pdMS_TO_TICKS(1000)); // Esperar antes de reintentar
        }
    }
    
    ESP_LOGE(TAG, "Fallo lectura DHT11 (err=%d)", err);
    return ESP_FAIL;
}

// Obtener histórico
void dht11_get_history(dht11_reading_t *buffer_out, int *count, int max_count) {
    int to_read = (buffer.count < max_count) ? buffer.count : max_count;
    
    for (int i = 0; i < to_read; i++) {
        int idx = (buffer.head + buffer.count - to_read + i) % DHT11_BUFFER_SIZE;
        buffer_out[i] = buffer.readings[idx];
    }
    
    *count = to_read;
}

// Tarea de muestreo
static void dht11_task(void *arg) {
    ESP_LOGI(TAG, "Tarea DHT11 iniciada");
    vTaskDelay(pdMS_TO_TICKS(2000)); // Esperar a que el sistema esté listo
    
    while (1) {
        float temp, humid;
        
        if (dht11_read(&temp, &humid) == ESP_OK) {
            // Agregar al buffer circular
            buffer.readings[buffer.head].temperature = temp;
            buffer.readings[buffer.head].humidity = humid;
            buffer.readings[buffer.head].timestamp = time(NULL);
            
            buffer.head = (buffer.head + 1) % DHT11_BUFFER_SIZE;
            if (buffer.count < DHT11_BUFFER_SIZE) {
                buffer.count++;
            }
            
            web_server_publish_metrics(temp, humid);
        }
        
        vTaskDelay(pdMS_TO_TICKS(8000)); // 8 segundos entre muestras
    }
}

// Iniciar tarea
esp_err_t dht11_start_task(void) {
    BaseType_t ret = xTaskCreate(
        dht11_task,
        "dht11_task",
        4096,
        NULL,
        tskIDLE_PRIORITY + 2, // PRIORITY_4 (menor que web_server)
        &dht11_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Error crear tarea DHT11");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
