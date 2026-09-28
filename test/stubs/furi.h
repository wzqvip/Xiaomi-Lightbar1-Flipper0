/* Minimal host stub so the pure protocol code builds without the Flipper SDK. */
#pragma once

#include <stdint.h>

static inline void furi_delay_ms(uint32_t ms) {
    (void)ms;
}

static inline void furi_delay_us(uint32_t us) {
    (void)us;
}

#define FURI_LOG_I(tag, fmt, ...) ((void)0)
#define FURI_LOG_W(tag, fmt, ...) ((void)0)
#define FURI_LOG_E(tag, fmt, ...) ((void)0)
#define FURI_LOG_D(tag, fmt, ...) ((void)0)
