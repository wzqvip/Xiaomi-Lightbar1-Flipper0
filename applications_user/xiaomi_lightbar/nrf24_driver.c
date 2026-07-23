#include "nrf24_driver.h"

#include <furi_hal.h>
#include <string.h>

#define NRF24_MAX_PAYLOAD_SIZE 32U
#define NRF24_TIMEOUT_MS 10U

#define NRF24_CMD_W_REGISTER 0x20U
#define NRF24_CMD_W_TX_PAYLOAD 0xA0U

#define NRF24_REG_CONFIG 0x00U
#define NRF24_REG_EN_AA 0x01U
#define NRF24_REG_RF_CH 0x05U
#define NRF24_REG_RF_SETUP 0x06U

#define NRF24_CONFIG_PWR_UP 0x02U
#define NRF24_CONFIG_PRIM_TX 0x00U

#define NRF24_RF_SETUP_2MBPS_0DBM 0x0EU

/*
 * Default GPIO wiring for external nRF24L01 module on Flipper Zero:
 *  - CSN: A4
 *  - CE : A7
 */
#define NRF24_PIN_CSN &gpio_ext_pa4
#define NRF24_PIN_CE &gpio_ext_pa7

/*
 * Minimal nRF24L01+ TX-only setup that matches the Xiaomi lightbar workflow:
 * - 2Mbps rate
 * - no auto-ack
 * - fast channel updates during command send
 *
 * NOTE:
 * This file intentionally keeps the SPI write path compact and self-contained
 * so app-side command logic stays simple.
 */

static bool nrf24_ready = false;
static uint8_t nrf24_channel = 6;

static bool nrf24_spi_tx(const uint8_t* data, size_t size) {
    furi_hal_spi_acquire(&furi_hal_spi_bus_handle_external);
    const bool ok =
        furi_hal_spi_bus_tx(&furi_hal_spi_bus_handle_external, data, size, NRF24_TIMEOUT_MS);
    furi_hal_spi_release(&furi_hal_spi_bus_handle_external);
    return ok;
}

static bool nrf24_hw_write_register(uint8_t reg, uint8_t value) {
    const uint8_t frame[] = {NRF24_CMD_W_REGISTER | (reg & 0x1F), value};
    furi_hal_gpio_write(NRF24_PIN_CSN, false);
    const bool ok = nrf24_spi_tx(frame, sizeof(frame));
    furi_hal_gpio_write(NRF24_PIN_CSN, true);
    return ok;
}

static bool nrf24_hw_write_payload(const uint8_t* data, uint8_t len) {
    uint8_t frame[1 + NRF24_MAX_PAYLOAD_SIZE];
    frame[0] = NRF24_CMD_W_TX_PAYLOAD;
    memcpy(&frame[1], data, len);

    furi_hal_gpio_write(NRF24_PIN_CSN, false);
    const bool ok = nrf24_spi_tx(frame, 1 + len);
    furi_hal_gpio_write(NRF24_PIN_CSN, true);
    return ok;
}

bool nrf24_begin(const Nrf24Config* config) {
    if(config) {
        nrf24_channel = config->rf_channel;
    }

    furi_hal_gpio_init_simple(NRF24_PIN_CSN, GpioModeOutputPushPull);
    furi_hal_gpio_init_simple(NRF24_PIN_CE, GpioModeOutputPushPull);
    furi_hal_gpio_write(NRF24_PIN_CSN, true);
    furi_hal_gpio_write(NRF24_PIN_CE, false);

    /*
     * Register sequence:
     *   CONFIG  = PWR_UP, PTX
     *   EN_AA   = 0x00 (disable auto-ack)
     *   RF_SETUP= 0x0E (2Mbps, 0dBm)
     *   RF_CH   = nrf24_channel
     */
    if(!nrf24_hw_write_register(NRF24_REG_CONFIG, NRF24_CONFIG_PWR_UP | NRF24_CONFIG_PRIM_TX)) {
        return false;
    }
    if(!nrf24_hw_write_register(NRF24_REG_EN_AA, 0x00)) return false;
    if(!nrf24_hw_write_register(NRF24_REG_RF_SETUP, NRF24_RF_SETUP_2MBPS_0DBM)) return false;
    if(!nrf24_hw_write_register(NRF24_REG_RF_CH, nrf24_channel)) return false;

    furi_delay_ms(2);
    furi_hal_gpio_write(NRF24_PIN_CE, true);

    nrf24_ready = true;
    return true;
}

void nrf24_end(void) {
    furi_hal_gpio_write(NRF24_PIN_CE, false);
    nrf24_ready = false;
}

bool nrf24_set_channel(uint8_t channel) {
    if(!nrf24_ready) return false;
    if(!nrf24_hw_write_register(NRF24_REG_RF_CH, channel)) return false;
    nrf24_channel = channel;
    return true;
}

bool nrf24_send_raw(const uint8_t* data, uint8_t len) {
    if(!nrf24_ready) return false;
    if(!data || len == 0 || len > NRF24_MAX_PAYLOAD_SIZE) return false;
    return nrf24_hw_write_payload(data, len);
}
