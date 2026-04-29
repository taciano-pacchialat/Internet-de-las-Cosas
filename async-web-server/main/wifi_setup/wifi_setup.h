#ifndef WIFI_SETUP_H
#define WIFI_SETUP_H

#include <esp_err.h>
#include <esp_netif.h>

// Inicializar WiFi y portal cautivo
esp_err_t wifi_setup_init(void);

// Verificar conexión
bool wifi_setup_is_connected(void);

// Obtener IP asignada
esp_ip4_addr_t wifi_setup_get_ip(void);

#endif // WIFI_SETUP_H
