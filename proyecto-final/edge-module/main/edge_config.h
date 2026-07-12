#pragma once

#include <cstdint>

// =============================================================================
// COMPATIBILIDAD ARDUINO / RADIOLIB
// =============================================================================
#ifndef LOW
#define LOW                         (0x0)
#endif
#ifndef HIGH
#define HIGH                        (0x1)
#endif
#ifndef INPUT
#define INPUT                       (0x01)
#endif
#ifndef OUTPUT
#define OUTPUT                      (0x03)
#endif
#ifndef RISING
#define RISING                      (0x01)
#endif
#ifndef FALLING
#define FALLING                     (0x02)
#endif
#ifndef NOP
#define NOP()                       asm volatile ("nop")
#endif

// =============================================================================
// CONFIGURACIÓN GENERAL
// =============================================================================
#define DEVICE_ID           "edge-01"

// Tiempo de deep sleep (en microsegundos)
// 30 segundos = 30 * 1000000
#define SLEEP_DURATION_US   (30ULL * 1000000ULL)

// Tiempo de espera RX tras transmitir heartbeat (ms)
#define RX_WAIT_TIMEOUT_MS  8000

// =============================================================================
// PINOUT VSPI — ESP32 DevKit V1 (Collar / Edge Module)
// GPIO18=SCK, GPIO19=MISO, GPIO23=MOSI (pines VSPI por defecto del ESP32)
// GPIO22=CS, GPIO14=RST, GPIO4=DIO0
// NOTA: Evitar GPIO0 (boot), GPIO2 (LED/boot), GPIO12 (boot-voltage), GPIO15 (boot), GPIO5 (boot pull-up)
// =============================================================================
#define LORA_SCK            18
#define LORA_MISO           19
#define LORA_MOSI           23
#define LORA_CS             22
#define LORA_RST            14
#define LORA_DIO0            4

// =============================================================================
// PARÁMETROS RF LORA (coincidentes con Gateway)
// =============================================================================
#define LORA_FREQ           433.0   // MHz
#define LORA_BW             125.0   // kHz
#define LORA_SF             9       // Spreading Factor (7-12)
#define LORA_CR             7       // Coding Rate (5-8)
#define LORA_SYNC_WORD      0x12
#define LORA_POWER          17      // dBm
#define LORA_PREAMBLE       8
