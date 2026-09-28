#ifndef NRF24_H
#define NRF24_H

#include <furi_hal_spi.h>
#include <furi_hal_gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Commands
#define R_REGISTER          0x00
#define W_REGISTER          0x20
#define R_RX_PAYLOAD        0x61
#define W_TX_PAYLOAD        0xA0
#define FLUSH_TX            0xE1
#define FLUSH_RX            0xE2
#define REUSE_TX_PL         0xE3
#define R_RX_PL_WID         0x60
#define W_ACK_PAYLOAD       0xA8
#define W_TX_PAYLOAD_NO_ACK 0xB0
#define NOP                 0xFF

// Registers
#define CONFIG      0x00
#define EN_AA       0x01
#define EN_RXADDR   0x02
#define SETUP_AW    0x03
#define SETUP_RETR  0x04
#define RF_CH       0x05
#define RF_SETUP    0x06
#define STATUS      0x07
#define OBSERVE_TX  0x08
#define RPD         0x09
#define RX_ADDR_P0  0x0A
#define RX_ADDR_P1  0x0B
#define RX_ADDR_P2  0x0C
#define RX_ADDR_P3  0x0D
#define RX_ADDR_P4  0x0E
#define RX_ADDR_P5  0x0F
#define TX_ADDR     0x10
#define RX_PW_P0    0x11
#define RX_PW_P1    0x12
#define RX_PW_P2    0x13
#define RX_PW_P3    0x14
#define RX_PW_P4    0x15
#define RX_PW_P5    0x16
#define FIFO_STATUS 0x17
#define DYNPD       0x1C
#define FEATURE     0x1D

// Register bits
#define MASK_RX_DR  6
#define MASK_TX_DS  5
#define MASK_MAX_RT 4
#define EN_CRC      3
#define CRCO        2
#define PWR_UP      1
#define PRIM_RX     0
#define ENAA_P5     5
#define ENAA_P4     4
#define ENAA_P3     3
#define ENAA_P2     2
#define ENAA_P1     1
#define ENAA_P0     0
#define ERX_P5      5
#define ERX_P4      4
#define ERX_P3      3
#define ERX_P2      2
#define ERX_P1      1
#define ERX_P0      0
#define AW          0
#define ARD         4
#define ARC         0
#define PLL_LOCK    4
#define RF_DR       3
#define RF_DR_LOW   5
#define RF_PWR      6
#define RX_DR       6
#define TX_DS       5
#define MAX_RT      4
#define RX_P_NO     1
#define TX_FULL     0
#define PLOS_CNT    4
#define ARC_CNT     0
#define TX_REUSE    6
#define FIFO_FULL   5
#define TX_EMPTY    4
#define RX_FULL_REG 1
#define RX_EMPTY    0
#define DPL_P5      5
#define DPL_P4      4
#define DPL_P3      3
#define DPL_P2      2
#define DPL_P1      1
#define DPL_P0      0
#define EN_DPL      2
#define EN_ACK_PAY  1
#define EN_DYN_ACK  0

#define NRF24_ADDRESS_LENGTH 5

typedef enum {
    NRF24_DATA_RATE_250K = 0,
    NRF24_DATA_RATE_1M,
    NRF24_DATA_RATE_2M
} Nrf24DataRate;

typedef enum {
    NRF24_PA_MIN = 0,
    NRF24_PA_LOW,
    NRF24_PA_HIGH,
    NRF24_PA_MAX
} Nrf24PaLevel;

void nrf24_init(void);
void nrf24_deinit(void);

bool nrf24_check_connected(void);

uint8_t nrf24_read_reg(uint8_t reg);
void nrf24_write_reg(uint8_t reg, uint8_t val);

void nrf24_read_reg_bytes(uint8_t reg, uint8_t* val, uint8_t len);
void nrf24_write_reg_bytes(uint8_t reg, const uint8_t* val, uint8_t len);

void nrf24_set_tx_address(const uint8_t* address, uint8_t len);
void nrf24_set_rx_address(uint8_t pipe, const uint8_t* address, uint8_t len);

void nrf24_set_rx_payload_width(uint8_t pipe, uint8_t width);

void nrf24_flush_rx(void);
void nrf24_flush_tx(void);

void nrf24_power_up(void);
void nrf24_power_down(void);

void nrf24_set_channel(uint8_t channel);
void nrf24_set_data_rate(Nrf24DataRate rate);
void nrf24_set_pa_level(Nrf24PaLevel level);

void nrf24_disable_crc(void);

void nrf24_set_auto_ack(bool enable);
void nrf24_set_dynamic_payloads(bool enable);
void nrf24_set_dynamic_ack(bool enable);

void nrf24_clear_status(void);

void nrf24_rx_mode(void);
void nrf24_tx_mode(void);

bool nrf24_available(void);
bool nrf24_read(uint8_t* buf, uint8_t len);

/* Returns true when the radio reported the packet as transmitted (TX_DS). */
bool nrf24_write(const uint8_t* buf, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif
