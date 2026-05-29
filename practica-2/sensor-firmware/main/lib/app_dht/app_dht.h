#ifndef _APP_DHT_H_
#define _APP_DHT_H_

#include "esp_err.h"

esp_err_t app_dht_init(int gpio_num);
esp_err_t app_dht_read_values(float *temp, float *hum);

#endif
