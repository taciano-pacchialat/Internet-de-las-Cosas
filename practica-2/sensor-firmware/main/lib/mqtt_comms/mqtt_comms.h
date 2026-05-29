#ifndef _MQTT_COMMS_H_
#define _MQTT_COMMS_H_

#include "esp_err.h"

esp_err_t mqtt_comms_init(const char *broker_url);
esp_err_t mqtt_comms_publish(const char *topic, const char *payload);

#endif
