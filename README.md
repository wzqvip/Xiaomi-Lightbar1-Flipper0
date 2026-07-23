# Xiaomi-Lightbar1-Flipper0

Control Xiaomi Lightbar 1 (not 1S) with Flipper Zero + nRF24L01.

## What is included

This repository now contains a minimal Flipper Zero external app (`.fap`) in:

- `/home/runner/work/Xiaomi-Lightbar1-Flipper0/Xiaomi-Lightbar1-Flipper0/applications_user/xiaomi_lightbar`

Implemented behavior:

- nRF24L01 TX setup path for Xiaomi Lightbar control flow
- 17-byte command frame builder:
  - preamble `53 39 14 DD 1C 49 34 12`
  - 3-byte remote ID
  - 2-byte command field
  - 2-byte rolling/padding bytes
  - CRC16 (CCITT, poly `0x1021`, init `0xFFFF`)
- channel hopping on send
- Flipper key mapping:
  - `OK`: power toggle
  - `UP/DOWN`: brightness up/down
  - `LEFT/RIGHT`: warmer/colder

## Wiring (Flipper Zero GPIO -> nRF24L01)

Use 3.3V only:

- `3V3` -> `VCC`
- `GND` -> `GND`
- `MOSI` -> `MOSI`
- `MISO` -> `MISO`
- `SCK` -> `SCK`
- `CS` -> `CSN`
- `CE` -> `CE`

## Build

Use `ufbt` with your Flipper firmware environment:

```bash
ufbt
```

Then deploy the built `.fap` to your Flipper and run **Xiaomi Lightbar** from the external apps menu.
