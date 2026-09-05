// Minimal mock of the LiquidCrystal_I2C library for host-based unit testing.
//
// It records CGRAM programming (createChar) and cell writes so tests can assert
// on what the rendering pipeline pushed to the display.
#ifndef ROBOEYES_TEST_MOCK_LIQUIDCRYSTAL_I2C_H
#define ROBOEYES_TEST_MOCK_LIQUIDCRYSTAL_I2C_H

#include <cstdint>
#include <array>
#include <vector>

class LiquidCrystal_I2C {
public:
  struct CreateCharCall {
    uint8_t slot;
    std::array<uint8_t, 8> rows;
  };
  struct WriteCall {
    uint8_t col;
    uint8_t row;
    uint8_t value;
  };

  uint8_t address;
  uint8_t cols;
  uint8_t rows;

  std::vector<CreateCharCall> createCharCalls;
  std::vector<WriteCall> writeCalls;
  uint8_t cursorCol = 0;
  uint8_t cursorRow = 0;
  bool initialized = false;
  bool backlightOn = false;

  LiquidCrystal_I2C(uint8_t addr, uint8_t c, uint8_t r)
      : address(addr), cols(c), rows(r) {}

  void init() { initialized = true; }
  void backlight() { backlightOn = true; }

  void createChar(uint8_t slot, uint8_t charmap[]) {
    CreateCharCall call;
    call.slot = slot;
    for (uint8_t i = 0; i < 8; i++) call.rows[i] = charmap[i];
    createCharCalls.push_back(call);
  }

  void setCursor(uint8_t col, uint8_t row) {
    cursorCol = col;
    cursorRow = row;
  }

  void write(uint8_t value) {
    writeCalls.push_back({cursorCol, cursorRow, value});
    cursorCol++;
  }

  // Test helper
  void resetLog() {
    createCharCalls.clear();
    writeCalls.clear();
  }
};

#endif  // ROBOEYES_TEST_MOCK_LIQUIDCRYSTAL_I2C_H
