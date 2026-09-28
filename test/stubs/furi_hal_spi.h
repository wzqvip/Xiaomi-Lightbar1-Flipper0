/*
 * Host stub: nrf24.h includes this but the protocol code never uses it.
 * The Flipper SDK pulls in <stdbool.h> through this header, so the stub must
 * do the same or nrf24.h's `bool` parameters fail to compile on the host.
 */
#pragma once

#include <stdbool.h>
