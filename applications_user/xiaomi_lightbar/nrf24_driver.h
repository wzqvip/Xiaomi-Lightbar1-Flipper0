#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t rf_channel;
} Nrf24Config;

bool nrf24_begin(const Nrf24Config* config);
void nrf24_end(void);
bool nrf24_set_channel(uint8_t channel);
bool nrf24_send_raw(const uint8_t* data, uint8_t len);
