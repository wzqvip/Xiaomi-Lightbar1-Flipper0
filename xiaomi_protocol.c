#include "xiaomi_protocol.h"
#include "nrf24.h"
#include <furi.h>
#include <stdio.h>

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

    // 1-byte Counter, then the 2-byte Command.
    // The counter comes FIRST -- this is easy to get backwards, and the bar
    // silently ignores every packet if you do.
    packet[12] = counter;
    packet[13] = (cmd >> 8) & 0xFF;
    packet[14] = cmd & 0xFF;

    // CRC16 covers all 15 preceding bytes, preamble included -- not just the
    // 7 bytes after the preamble.
    uint16_t crc = xiaomi_crc16(packet, 15);
    packet[15] = (crc >> 8) & 0xFF;
    packet[16] = crc & 0xFF;
}

void xiaomi_send_command(uint32_t remote_id, uint16_t cmd, uint8_t* counter) {
    uint8_t packet[17];
    xiaomi_build_packet(packet, remote_id, cmd, *counter);
    *counter = (*counter + 1) % 256;

    // Transmit across the 4 primary channels to ensure the receiver gets it
    uint8_t channels[] = {6, 15, 43, 68};
    uint8_t sent = 0;
    for (int c = 0; c < 4; c++) {
        nrf24_set_channel(channels[c]);
        for (int i = 0; i < 15; i++) {
            if (nrf24_write(packet, 17)) {
                sent++;
            }
            // Keep the ~5 ms cadence this app was verified with. nrf24_write()
            // only waits for TX_DS (~150 us), it does not space the packets.
            furi_delay_ms(4);
        }
    }

    // Handy when debugging over the CLI: `log` shows this after a key press.
    char hex[17 * 3 + 1];
    for (size_t i = 0; i < sizeof(packet); i++) {
        snprintf(hex + i * 3, 4, "%02X ", packet[i]);
    }
    FURI_LOG_I("XiaomiLB", "id=%06lX cmd=%04X n=%u %s(%u/60 sent)",
               (unsigned long)remote_id, cmd, (unsigned)*counter - 1u, hex, sent);
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
