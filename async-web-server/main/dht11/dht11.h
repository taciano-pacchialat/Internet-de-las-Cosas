#ifndef DHT11_H
#define DHT11_H

#include <esp_err.h>
#include <time.h>

// Estructura para lectura DHT11
typedef struct {
    float temperature;   // Celsius
    float humidity;      // Porcentaje
    time_t timestamp;    // Época
} dht11_reading_t;

// Inicializar DHT11 en GPIO 4
esp_err_t dht11_init(void);

// Leer sensor (bloqueante, ~18ms)
esp_err_t dht11_read(float *temperature, float *humidity);

// Obtener últimas N lecturas
void dht11_get_history(dht11_reading_t *buffer, int *count, int max_count);

// Iniciar tarea de muestreo
esp_err_t dht11_start_task(void);

#endif // DHT11_H
