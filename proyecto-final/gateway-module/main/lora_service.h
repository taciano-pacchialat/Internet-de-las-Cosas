#pragma once

#include <cstddef>

// =============================================================================
// SERVICIO LORA DEL GATEWAY MODULE (HSPI)
// =============================================================================

bool lora_init(void);
bool lora_receive(char* buffer, size_t max_len, float* rssi, float* snr);
void lora_send_downlink(const char* data);
void lora_set_rx_and_prepare_sleep(void);
void lora_spi_end(void);
