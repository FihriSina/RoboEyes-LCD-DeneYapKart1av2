// Definitions for the mock Arduino runtime globals.
#include "Arduino.h"
#include "Wire.h"

namespace mock_arduino {
unsigned long g_millis = 0;
uint32_t g_rng_state = 1;

// xorshift32 – small, fast, deterministic.
uint32_t next_random() {
  uint32_t x = g_rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  g_rng_state = x;
  return x;
}
}  // namespace mock_arduino

MockSerial Serial;
MockWire Wire;
