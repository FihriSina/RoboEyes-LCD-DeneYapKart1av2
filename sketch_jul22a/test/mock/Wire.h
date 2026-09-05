// Minimal mock of the Arduino Wire (I2C) library for host-based unit testing.
#ifndef ROBOEYES_TEST_MOCK_WIRE_H
#define ROBOEYES_TEST_MOCK_WIRE_H

#include <cstdint>

class MockWire {
public:
  // endTransmission() returns 0 on success. Tests flip this to emulate a
  // missing / unresponsive LCD.
  uint8_t endTransmissionResult = 0;
  uint8_t lastAddress = 0;
  bool beganTransmission = false;

  void begin(int, int) {}
  void setClock(uint32_t) {}

  void beginTransmission(uint8_t address) {
    lastAddress = address;
    beganTransmission = true;
  }

  uint8_t endTransmission() {
    beganTransmission = false;
    return endTransmissionResult;
  }
};

extern MockWire Wire;

#endif  // ROBOEYES_TEST_MOCK_WIRE_H
