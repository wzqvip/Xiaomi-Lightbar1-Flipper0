#include "xiaomi_protocol.h"
#include "nrf24.h"
#include <furi.h>

uint16_t xiaomi_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFE;
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

void xiaomi_build_packet(uint8_t* packet, uint32_t remote_id, uint16_t cmd, uint8_t counter) {
    // 8-byte Preamble
    packet[0] = 0x53;
    packet[1] = 0x39;
    packet[2] = 0x14;
    packet[3] = 0xDD;
    packet[4] = 0x1C;
    packet[5] = 0x49;
    packet[6] = 0x34;
    packet[7] = 0x12;

    // 3-byte Remote ID
    packet[8] = (remote_id >> 16) & 0xFF;
    packet[9] = (remote_id >> 8) & 0xFF;
    packet[10] = remote_id & 0xFF;

    // 1-byte Separator
    packet[11] = 0xFF;

    // 2-byte Command
    packet[12] = (cmd >> 8) & 0xFF;
    packet[13] = cmd & 0xFF;

    // 1-byte Counter
    packet[14] = counter;

    // Calculate CRC16 on packet[8..14] (7 bytes)
    uint16_t crc = xiaomi_crc16(&packet[8], 7);
    packet[15] = (crc >> 8) & 0xFF;
    packet[16] = crc & 0xFF;
}

void xiaomi_send_command(uint32_t remote_id, uint16_t cmd, uint8_t* counter) {
    uint8_t packet[17];
    xiaomi_build_packet(packet, remote_id, cmd, *counter);
    *counter = (*counter + 1) % 256;

    // Transmit across the 4 primary channels to ensure the receiver gets it
    uint8_t channels[] = {6, 15, 43, 68};
    for (int c = 0; c < 4; c++) {
        nrf24_set_channel(channels[c]);
        for (int i = 0; i < 15; i++) {
            nrf24_write(packet, 17);
            furi_delay_ms(5);
        }
    }
}

static uint8_t clamp(uint8_t x) {
    if (x < 1) return 1;
    if (x > 15) return 15;
    return x;
}

uint16_t cmd_cooler(uint8_t steps) {
    return 0x0200 + clamp(steps);
}

uint16_t cmd_warmer(uint8_t steps) {
    return 0x0300 + (256 - clamp(steps));
}

uint16_t cmd_stronger(uint8_t steps) {
    return 0x0400 + clamp(steps);
}

uint16_t cmd_softer(uint8_t steps) {
    return 0x0500 + (256 - clamp(steps));
}
