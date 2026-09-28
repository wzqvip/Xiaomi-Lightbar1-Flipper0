#include "nrf24.h"
#include <furi_hal_resources.h>

// Define the GPIO pins for CE and CSN (using the external SPI bus)
static FuriHalSpiBusHandle* spi = (FuriHalSpiBusHandle*)&furi_hal_spi_bus_handle_external;

// Pin definitions
// CE pin (PB2 / Pin 6)
#define NRF24_CE_PIN &gpio_ext_pb2
// CSN pin (PC3 / Pin 13)
#define NRF24_CSN_PIN &gpio_ext_pc3

static void nrf24_ce_high(void) {
    furi_hal_gpio_write(NRF24_CE_PIN, true);
}

static void nrf24_ce_low(void) {
    furi_hal_gpio_write(NRF24_CE_PIN, false);
}

static void nrf24_csn_high(void) {
    furi_hal_gpio_write(NRF24_CSN_PIN, true);
}

static void nrf24_csn_low(void) {
    furi_hal_gpio_write(NRF24_CSN_PIN, false);
}

void nrf24_init(void) {
    furi_hal_gpio_init(NRF24_CE_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    nrf24_ce_low();
    
    furi_hal_gpio_init(NRF24_CSN_PIN, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    nrf24_csn_high();
}

void nrf24_deinit(void) {
    nrf24_power_down();
    furi_hal_gpio_init(NRF24_CE_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(NRF24_CSN_PIN, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
}

static uint8_t nrf24_spi_transfer(uint8_t data) {
    uint8_t rx_data = 0;
    furi_hal_spi_bus_trx(spi, &data, &rx_data, 1, 100);
    return rx_data;
}

uint8_t nrf24_read_reg(uint8_t reg) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    uint8_t status = nrf24_spi_transfer(R_REGISTER | reg);
    uint8_t val = nrf24_spi_transfer(NOP);
    nrf24_csn_high();
    furi_hal_spi_release(spi);
    (void)status;
    return val;
}

void nrf24_write_reg(uint8_t reg, uint8_t val) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    uint8_t status = nrf24_spi_transfer(W_REGISTER | reg);
    nrf24_spi_transfer(val);
    nrf24_csn_high();
    furi_hal_spi_release(spi);
    (void)status;
}

void nrf24_read_reg_bytes(uint8_t reg, uint8_t* val, uint8_t len) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    uint8_t status = nrf24_spi_transfer(R_REGISTER | reg);
    for (uint8_t i = 0; i < len; i++) {
        val[i] = nrf24_spi_transfer(NOP);
    }
    nrf24_csn_high();
    furi_hal_spi_release(spi);
    (void)status;
}

void nrf24_write_reg_bytes(uint8_t reg, const uint8_t* val, uint8_t len) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    uint8_t status = nrf24_spi_transfer(W_REGISTER | reg);
    for (uint8_t i = 0; i < len; i++) {
        nrf24_spi_transfer(val[i]);
    }
    nrf24_csn_high();
    furi_hal_spi_release(spi);
    (void)status;
}

bool nrf24_check_connected(void) {
    uint8_t val = nrf24_read_reg(SETUP_AW);
    if (val == 0x03 || val == 0x01 || val == 0x02) { // 3, 4 or 5 bytes address width
        return true;
    }
    return false;
}

void nrf24_set_tx_address(const uint8_t* address, uint8_t len) {
    nrf24_write_reg_bytes(TX_ADDR, address, len);
}

void nrf24_set_rx_address(uint8_t pipe, const uint8_t* address, uint8_t len) {
    if (pipe > 5) return;
    nrf24_write_reg_bytes(RX_ADDR_P0 + pipe, address, len);
}

void nrf24_set_rx_payload_width(uint8_t pipe, uint8_t width) {
    if (pipe > 5) return;
    nrf24_write_reg(RX_PW_P0 + pipe, width);
}

void nrf24_flush_rx(void) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    nrf24_spi_transfer(FLUSH_RX);
    nrf24_csn_high();
    furi_hal_spi_release(spi);
}

void nrf24_flush_tx(void) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    nrf24_spi_transfer(FLUSH_TX);
    nrf24_csn_high();
    furi_hal_spi_release(spi);
}

void nrf24_power_up(void) {
    uint8_t val = nrf24_read_reg(CONFIG);
    if (!(val & (1 << PWR_UP))) {
        nrf24_write_reg(CONFIG, val | (1 << PWR_UP));
        furi_delay_ms(5); // Wait for boot
    }
}

void nrf24_power_down(void) {
    uint8_t val = nrf24_read_reg(CONFIG);
    nrf24_write_reg(CONFIG, val & ~(1 << PWR_UP));
}

void nrf24_set_channel(uint8_t channel) {
    nrf24_write_reg(RF_CH, channel & 0x7F);
}

void nrf24_set_data_rate(Nrf24DataRate rate) {
    uint8_t val = nrf24_read_reg(RF_SETUP);
    val &= ~((1 << RF_DR) | (1 << RF_DR_LOW)); // Clear both data rate bits
    if (rate == NRF24_DATA_RATE_250K) {
        val |= (1 << RF_DR_LOW);
    } else if (rate == NRF24_DATA_RATE_2M) {
        val |= (1 << RF_DR);
    }
    nrf24_write_reg(RF_SETUP, val);
}

void nrf24_set_pa_level(Nrf24PaLevel level) {
    uint8_t val = nrf24_read_reg(RF_SETUP);
    val &= ~0x06; // Clear PA bits (1 and 2)
    val |= ((level & 0x03) << 1);
    nrf24_write_reg(RF_SETUP, val);
}

void nrf24_disable_crc(void) {
    // The light bar has no nRF24 on the other end, so the chip's own CRC is
    // pure overhead. Both reference implementations turn it off; leaving it on
    // appends extra bytes after the 17 byte payload.
    uint8_t val = nrf24_read_reg(CONFIG);
    nrf24_write_reg(CONFIG, val & ~((1 << EN_CRC) | (1 << CRCO)));
}

void nrf24_set_auto_ack(bool enable) {
    if (enable) {
        nrf24_write_reg(EN_AA, 0x3F); // Enable on all pipes
    } else {
        nrf24_write_reg(EN_AA, 0x00);
    }
}

void nrf24_set_dynamic_payloads(bool enable) {
    if (enable) {
        nrf24_write_reg(FEATURE, nrf24_read_reg(FEATURE) | (1 << EN_DPL));
        nrf24_write_reg(DYNPD, 0x3F);
    } else {
        nrf24_write_reg(FEATURE, nrf24_read_reg(FEATURE) & ~(1 << EN_DPL));
        nrf24_write_reg(DYNPD, 0x00);
    }
}

void nrf24_set_dynamic_ack(bool enable) {
    if (enable) {
        nrf24_write_reg(FEATURE, nrf24_read_reg(FEATURE) | (1 << EN_DYN_ACK));
    } else {
        nrf24_write_reg(FEATURE, nrf24_read_reg(FEATURE) & ~(1 << EN_DYN_ACK));
    }
}

void nrf24_clear_status(void) {
    nrf24_write_reg(STATUS, (1 << RX_DR) | (1 << TX_DS) | (1 << MAX_RT));
}

void nrf24_rx_mode(void) {
    nrf24_ce_low();
    uint8_t val = nrf24_read_reg(CONFIG);
    nrf24_write_reg(CONFIG, val | (1 << PRIM_RX));
    nrf24_ce_high();
    nrf24_clear_status();
}

void nrf24_tx_mode(void) {
    nrf24_ce_low();
    uint8_t val = nrf24_read_reg(CONFIG);
    nrf24_write_reg(CONFIG, val & ~(1 << PRIM_RX));
    nrf24_clear_status();
}

bool nrf24_available(void) {
    uint8_t val = nrf24_read_reg(FIFO_STATUS);
    return !(val & (1 << RX_EMPTY));
}

bool nrf24_read(uint8_t* buf, uint8_t len) {
    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    uint8_t status = nrf24_spi_transfer(R_RX_PAYLOAD);
    for (uint8_t i = 0; i < len; i++) {
        buf[i] = nrf24_spi_transfer(NOP);
    }
    nrf24_csn_high();
    furi_hal_spi_release(spi);
    (void)status;
    nrf24_write_reg(STATUS, (1 << RX_DR)); // Clear RX_DR bit
    return true;
}

bool nrf24_write(const uint8_t* buf, uint8_t len) {
    nrf24_flush_tx();

    furi_hal_spi_acquire(spi);
    nrf24_csn_low();
    nrf24_spi_transfer(W_TX_PAYLOAD_NO_ACK);
    for(uint8_t i = 0; i < len; i++) {
        nrf24_spi_transfer(buf[i]);
    }
    nrf24_csn_high();
    furi_hal_spi_release(spi);

    /*
     * CE has to stay high for at least 10 us to kick off the transmission.
     * Rather than sleeping blindly afterwards, poll STATUS: flushing the TX
     * FIFO while a packet is still in flight would abort it.
     */
    nrf24_ce_high();
    furi_delay_us(20);
    nrf24_ce_low();

    for(uint8_t i = 0; i < 40; i++) {
        uint8_t status = nrf24_read_reg(STATUS);
        if(status & ((1 << TX_DS) | (1 << MAX_RT))) {
            nrf24_write_reg(STATUS, (1 << TX_DS) | (1 << MAX_RT));
            return (status & (1 << TX_DS)) != 0;
        }
        furi_delay_us(20);
    }
    return false;
}
