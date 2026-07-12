#pragma once

// =============================================================================
// SERVICIO MQTT DEL GATEWAY MODULE (con resolución mDNS dinámica)
// =============================================================================

void mqtt_service_init_async(void);
void mqtt_init_and_publish(const char* heartbeat_payload);
bool mqtt_is_ready(void);

bool mqtt_has_new_gps(void);
bool mqtt_has_new_fence(void);
const char* mqtt_get_downlink_gps(void);
const char* mqtt_get_downlink_fence(void);
void mqtt_reset_flags(void);
