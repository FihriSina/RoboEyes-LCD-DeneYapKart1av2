// Minimal mock of ESP-IDF esp_system.h for host-based unit testing.
#ifndef ROBOEYES_TEST_MOCK_ESP_SYSTEM_H
#define ROBOEYES_TEST_MOCK_ESP_SYSTEM_H

#include <cstdint>

// Deterministic stand-in for the hardware RNG entropy source.
inline uint32_t esp_random() { return 0x1234abcdu; }

#endif  // ROBOEYES_TEST_MOCK_ESP_SYSTEM_H
