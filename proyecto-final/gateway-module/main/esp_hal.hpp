#pragma once

#include <RadioLib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "gateway_config.h"

// =============================================================================
// HAL ESP-IDF para RadioLib (HSPI / SPI2_HOST para ESP32-CAM)
// =============================================================================
class EspHal : public RadioLibHal {
public:
    EspHal(int8_t sck, int8_t miso, int8_t mosi)
        : RadioLibHal(INPUT, OUTPUT, LOW, HIGH, RISING, FALLING),
          _sck(sck), _miso(miso), _mosi(mosi), _spiHandle(nullptr) {}

    void pinMode(uint32_t pin, uint32_t mode) override {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin);
        io_conf.mode = (mode == OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);
    }

    void digitalWrite(uint32_t pin, uint32_t value) override {
        gpio_set_level((gpio_num_t)pin, value);
    }

    uint32_t digitalRead(uint32_t pin) override {
        return gpio_get_level((gpio_num_t)pin);
    }

    void attachInterrupt(uint32_t pin, void (*isr)(void), uint32_t mode) override {}
    void detachInterrupt(uint32_t pin) override {}

    void delay(unsigned long ms) override {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }

    void delayMicroseconds(unsigned long us) override {
        esp_rom_delay_us(us);
    }

    unsigned long millis() override {
        return (unsigned long)(esp_timer_get_time() / 1000);
    }

    unsigned long micros() override {
        return (unsigned long)esp_timer_get_time();
    }

    long pulseIn(uint32_t pin, uint32_t state, unsigned long timeout) override {
        return 0;
    }

    void spiBegin() override {
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = _mosi;
        bus_cfg.miso_io_num = _miso;
        bus_cfg.sclk_io_num = _sck;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 256;

        // Usar HSPI_HOST (SPI2_HOST) para ESP32-CAM (evita conflicto con PSRAM en VSPI)
        esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE("EspHal", "SPI bus init failed: %s", esp_err_to_name(ret));
        }
    }

    void spiBeginTransaction() override {}

    void spiTransfer(uint8_t* out, size_t len, uint8_t* in) override {
        spi_transaction_t t = {};
        t.length = len * 8;
        t.tx_buffer = out;
        t.rx_buffer = in;
        spi_device_transmit(_spiHandle, &t);
    }

    void spiEndTransaction() override {}

    void spiEnd() override {
        if (_spiHandle) {
            spi_bus_remove_device(_spiHandle);
            _spiHandle = nullptr;
        }
        spi_bus_free(SPI2_HOST);
    }

    void spiAddDevice(int csPin) {
        spi_device_interface_config_t dev_cfg = {};
        dev_cfg.clock_speed_hz = 2 * 1000 * 1000;
        dev_cfg.mode = 0;
        dev_cfg.spics_io_num = csPin;
        dev_cfg.queue_size = 1;

        esp_err_t ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &_spiHandle);
        if (ret != ESP_OK) {
            ESP_LOGE("EspHal", "SPI add device failed: %s", esp_err_to_name(ret));
        }
    }

private:
    int8_t _sck, _miso, _mosi;
    spi_device_handle_t _spiHandle;
};
