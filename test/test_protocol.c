/*
 * Host-side unit test for the Xiaomi light bar packet builder.
 *
 * xiaomi_crc16() and xiaomi_build_packet() are pure functions, so they can be
 * compiled and checked on a PC without the Flipper SDK. The expected values are
 * the ground truth from lamperez's reverse engineering, cross-checked with
 * `reveng`:
 *
 *   reveng -w 16 -s 533914DD1C49341201B960FF7901003870 \
 *                   533914DD1C49341201B960FF1601008F2A \
 *                   533914DD1C49341201B960FF1A0100FA4B \
 *                   533914DD1C49341201B960FF200100F82F
 *   width=16 poly=0x1021 init=0xfffe refin=false refout=false xorout=0x0000
 *
 * Build & run (from the repository root):
 *   gcc -std=c11 -Wall -Wextra -I. -Itest/stubs \
 *       -o test/test_protocol.exe test/test_protocol.c xiaomi_protocol.c
 *   ./test/test_protocol.exe
 */

#include <stdio.h>
#include <string.h>

#include "xiaomi_protocol.h"
#include "nrf24.h"

/*
 * Link stubs. xiaomi_send_command() calls into the radio layer, but these
 * tests only exercise the pure packet/crc helpers and never transmit.
 */
void nrf24_set_channel(uint8_t channel) {
    (void)channel;
}
void nrf24_write(const uint8_t* buf, uint8_t len) {
    (void)buf;
    (void)len;
}

static int checks = 0;
static int failures = 0;

static void hex_to_bytes(const char* hex, uint8_t* out, size_t out_len) {
    for(size_t i = 0; i < out_len; i++) {
        unsigned value = 0;
        sscanf(hex + i * 2, "%2x", &value);
        out[i] = (uint8_t)value;
    }
}

static void print_hex(const char* label, const uint8_t* data, size_t len) {
    printf("      %-10s", label);
    for(size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
        if(i == 7 || i == 10 || i == 11 || i == 12 || i == 14) printf(" ");
    }
    printf("\n");
}

/*
 * The 4 captured packets from the reverse engineering notes. Each one is the
 * full on-air payload: 8 byte preamble, 3 byte id, 0xFF separator, 1 byte
 * counter, 2 byte command, 2 byte CRC.
 *
 * Note the field order: the counter comes BEFORE the command, and the CRC
 * covers all 15 preceding bytes (preamble included) -- not just the 7 bytes
 * after the preamble.
 */
static const struct {
    const char* body15; /* preamble + id + separator + counter + command */
    uint16_t expect_crc;
} VECTORS[] = {
    {"533914DD1C49341201B960FF790100", 0x3870},
    {"533914DD1C49341201B960FF160100", 0x8F2A},
    {"533914DD1C49341201B960FF1A0100", 0xFA4B},
    {"533914DD1C49341201B960FF200100", 0xF82F},
};
#define VECTOR_COUNT (sizeof(VECTORS) / sizeof(VECTORS[0]))

static void test_crc_vectors(void) {
    printf("[1] CRC16 over the 4 reference packets\n");
    for(size_t i = 0; i < VECTOR_COUNT; i++) {
        uint8_t body[15];
        hex_to_bytes(VECTORS[i].body15, body, sizeof(body));
        uint16_t got = xiaomi_crc16(body, sizeof(body));
        checks++;
        int ok = (got == VECTORS[i].expect_crc);
        if(!ok) failures++;
        printf("  %s  %s  crc=0x%04X expected=0x%04X\n",
               ok ? "PASS" : "FAIL",
               VECTORS[i].body15,
               got,
               VECTORS[i].expect_crc);
    }
}

static void test_build_packet(void) {
    printf("\n[2] xiaomi_build_packet() output vs the reference packets\n");
    for(size_t i = 0; i < VECTOR_COUNT; i++) {
        uint8_t expected[17];
        hex_to_bytes(VECTORS[i].body15, expected, 15);
        expected[15] = (uint8_t)(VECTORS[i].expect_crc >> 8);
        expected[16] = (uint8_t)(VECTORS[i].expect_crc & 0xFF);

        /* id and counter are recovered from the reference body */
        uint32_t id = ((uint32_t)expected[8] << 16) | ((uint32_t)expected[9] << 8) | expected[10];
        uint8_t counter = expected[12];
        uint16_t cmd = ((uint16_t)expected[13] << 8) | expected[14];

        uint8_t actual[17];
        memset(actual, 0, sizeof(actual));
        xiaomi_build_packet(actual, id, cmd, counter);

        checks++;
        int ok = (memcmp(actual, expected, sizeof(expected)) == 0);
        if(!ok) failures++;
        printf("  %s  id=0x%06lX cmd=0x%04X counter=%u\n",
               ok ? "PASS" : "FAIL", (unsigned long)id, cmd, counter);
        if(!ok) {
            print_hex("expected", expected, 17);
            print_hex("actual  ", actual, 17);
        }
    }
}

static void test_command_codes(void) {
    printf("\n[3] command code helpers\n");
    /*
     * The turning commands count DOWN from the next base, so that step 1
     * reproduces the default code the original remote sends:
     *   warmer(1) = 0x0400 - 1 = 0x03FF   (documented default)
     *   lower(1)  = 0x0600 - 1 = 0x05FF   (documented default)
     * Equivalently base + (256 - steps), which is how the code writes it.
     */
    struct {
        const char* name;
        uint16_t got;
        uint16_t want;
    } cases[] = {
        {"cooler(1)      -> 0x0201", cmd_cooler(1), 0x0201},
        {"cooler(15)     -> 0x020F", cmd_cooler(15), 0x020F},
        {"warmer(1)      -> 0x03FF", cmd_warmer(1), 0x03FF},
        {"warmer(15)     -> 0x03F1", cmd_warmer(15), 0x03F1},
        {"stronger(1)    -> 0x0401", cmd_stronger(1), 0x0401},
        {"stronger(15)   -> 0x040F", cmd_stronger(15), 0x040F},
        {"softer(1)      -> 0x05FF", cmd_softer(1), 0x05FF},
        {"softer(15)     -> 0x05F1", cmd_softer(15), 0x05F1},
    };
    for(size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        checks++;
        int ok = (cases[i].got == cases[i].want);
        if(!ok) failures++;
        printf("  %s  %s (got 0x%04X)\n",
               ok ? "PASS" : "FAIL", cases[i].name, cases[i].got);
    }
}

int main(void) {
    printf("Xiaomi light bar protocol tests (host build)\n");
    printf("packet layout expected: preamble8 | id3 | FF | counter | cmd2 | crc2\n\n");

    test_crc_vectors();
    test_build_packet();
    test_command_codes();

    printf("\n%d/%d checks passed\n", checks - failures, checks);
    if(failures) printf("%d FAILED\n", failures);
    return failures ? 1 : 0;
}
