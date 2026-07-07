#pragma once

// =============================================================================
// COMUNICACIÓN LORA (SX1278) DEL EDGE MODULE
// =============================================================================

bool lora_init(void);
void lora_send_heartbeat(bool inside);
void lora_wait_for_downlink(void);
void lora_cleanup(void);
