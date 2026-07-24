#ifndef XIAOMI_PROTOCOL_H
#define XIAOMI_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define PACKET_SIZE 17

// Commands
#define CMD_ON_OFF      0x0100
#define CMD_RESET       0x0600

uint16_t xiaomi_crc16(const uint8_t* data, size_t len);
void xiaomi_build_packet(uint8_t* packet, uint32_t remote_id, uint16_t cmd, uint8_t counter);
void xiaomi_send_command(uint32_t remote_id, uint16_t cmd, uint8_t* counter);

// Helper command generators
uint16_t cmd_cooler(uint8_t steps);
uint16_t cmd_warmer(uint8_t steps);
uint16_t cmd_stronger(uint8_t steps);
uint16_t cmd_softer(uint8_t steps);

#endif
