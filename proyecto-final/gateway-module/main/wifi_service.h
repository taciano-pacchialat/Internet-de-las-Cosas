#pragma once

#include <cstdint>

// =============================================================================
// GESTIÓN DE CONEXIÓN WI-FI DEL GATEWAY MODULE (esp32-wifi-manager & mDNS)
// =============================================================================

void wifi_service_init(void);
bool wifi_is_connected(void);
void wifi_wait_for_connection(uint32_t timeout_ms);
void wifi_disconnect_and_deinit(void);
