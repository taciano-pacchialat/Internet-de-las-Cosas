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
// CONFIGURACIÓN DE RED (WI-FI & MQTT)
// =============================================================================
#define WIFI_SSID                "TU_SSID_AQUI"
#define WIFI_PASS                "TU_PASSWORD_AQUI"
#define WIFI_CONNECT_TIMEOUT_MS  10000  // 10 segundos máximo para conectar
#define MAX_WIFI_RETRY           3

#define MQTT_BROKER_URI          "mqtt://192.168.1.100:1883"  // Cambiar a IP del server
#define MQTT_TOPIC_HEARTBEAT     "geofence/heartbeat"
#define MQTT_TOPIC_SIM_GPS       "geofence/simulated_gps"
#define MQTT_TOPIC_FENCE_UPDATE  "geofence/fence_update"
#define MQTT_SUBSCRIBE_WAIT_MS   3000   // Espera máxima para recibir mensajes retenidos

// =============================================================================
// PINOUT VSPI — ESP32 Clásico (Gateway Module)
// =============================================================================
#define LORA_SCK     18
#define LORA_MISO    19
#define LORA_MOSI    23
#define LORA_CS       5
#define LORA_RST     14
#define LORA_DIO0    26

// =============================================================================
// PARÁMETROS RF LORA (coincidentes con Edge Module)
// =============================================================================
#define LORA_FREQ        433.0   // MHz
#define LORA_BW          125.0   // kHz
#define LORA_SF          9       // Spreading Factor (7-12)
#define LORA_CR          7       // Coding Rate (5-8)
#define LORA_SYNC_WORD   0x12
#define LORA_POWER       17      // dBm
#define LORA_PREAMBLE    8
