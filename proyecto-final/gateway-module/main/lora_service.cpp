#include "lora_service.h"
#include <cstdio>
#include "esp_log.h"
#include "gateway_config.h"
#include "esp_hal.hpp"

static const char* TAG = "LORA_GW";

static EspHal* hal = nullptr;
static SX1278* lora = nullptr;
static Module* mod = nullptr;

bool lora_init(void) {
    hal = new EspHal(LORA_SCK, LORA_MISO, LORA_MOSI);
    mod = new Module(hal, LORA_CS, LORA_DIO0, LORA_RST, RADIOLIB_NC);
    lora = new SX1278(mod);

    hal->spiBegin();
    hal->spiAddDevice(LORA_CS);

    ESP_LOGI(TAG, "Inicializando SX1278 en VSPI...");
    int state = lora->begin(LORA_FREQ, LORA_BW, LORA_SF, LORA_CR,
                            LORA_SYNC_WORD, LORA_POWER, LORA_PREAMBLE);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGE(TAG, "SX1278 init falló, código: %d", state);
        return false;
    }
    ESP_LOGI(TAG, "SX1278 inicializado OK");
    return true;
}

bool lora_receive(char* buffer, size_t max_len, float* rssi, float* snr) {
    int state = lora->readData((uint8_t*)buffer, max_len - 1);

    if (state == RADIOLIB_ERR_NONE) {
        size_t len = lora->getPacketLength();
        if (len >= max_len) len = max_len - 1;
        buffer[len] = '\0';

        *rssi = lora->getRSSI();
        *snr  = lora->getSNR();

        ESP_LOGI(TAG, "LoRa RX: %s (RSSI=%.1f, SNR=%.1f)", buffer, *rssi, *snr);
        return true;
    } else {
        ESP_LOGW(TAG, "LoRa readData falló, código: %d", state);
        return false;
    }
}

void lora_send_downlink(const char* data) {
    ESP_LOGI(TAG, "LoRa TX downlink: %s", data);
    lora->standby();
    int state = lora->transmit(data);
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "LoRa TX falló, código: %d", state);
    }
}

void lora_set_rx_and_prepare_sleep(void) {
    int state = lora->startReceive();
    if (state != RADIOLIB_ERR_NONE) {
        ESP_LOGW(TAG, "startReceive falló: %d", state);
    }
    ESP_LOGI(TAG, "SX1278 en modo RX continuo, DIO0 disparará wakeup");
}

void lora_spi_end(void) {
    // NOTA: No hacer lora_cleanup() completo porque el SX1278 debe
    // seguir encendido en RX. Solo desmontar el driver SPI del ESP32.
    if (hal) {
        hal->spiEnd();
    }
}
