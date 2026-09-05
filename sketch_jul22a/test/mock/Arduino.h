// Minimal Arduino core mock for host-based (native) unit testing.
//
// This header reproduces just enough of the Arduino runtime that the RoboEyes
// sketch relies on so that its pure logic can be compiled and exercised with a
// standard C++ toolchain (g++) off the microcontroller.
//
// The goal is deterministic, inspectable behaviour:
//   * millis() is driven by a value the tests set explicitly.
//   * random() is a seedable linear congruential generator so stochastic code
//     paths run reproducibly.
//   * Serial is backed by an in-memory input/output buffer.

#ifndef ROBOEYES_TEST_MOCK_ARDUINO_H
#define ROBOEYES_TEST_MOCK_ARDUINO_H

#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <string>
#include <deque>

// ---------------------------------------------------------------------------
// Binary literal macros used by the sketch (Arduino provides B00000..B11111111)
// ---------------------------------------------------------------------------
#ifndef B00001
#define B00001 0x01
#endif
#ifndef B11000
#define B11000 0x18
#endif
#ifndef B10000
#define B10000 0x10
#endif
#ifndef B11111
#define B11111 0x1F
#endif

// ---------------------------------------------------------------------------
// Pin aliases referenced in setup()
// ---------------------------------------------------------------------------
#ifndef SDA
#define SDA 21
#endif
#ifndef SCL
#define SCL 22
#endif

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
namespace mock_arduino {
extern unsigned long g_millis;
}

inline unsigned long millis() { return mock_arduino::g_millis; }

// ---------------------------------------------------------------------------
// Pseudo-random number generator (deterministic, seedable)
// ---------------------------------------------------------------------------
namespace mock_arduino {
extern uint32_t g_rng_state;
uint32_t next_random();
}

inline void randomSeed(unsigned long seed) {
  mock_arduino::g_rng_state = static_cast<uint32_t>(seed ? seed : 1);
}

// random(max): 0 .. max-1
inline long random(long howbig) {
  if (howbig <= 0) return 0;
  return static_cast<long>(mock_arduino::next_random() % static_cast<uint32_t>(howbig));
}

// random(min, max): min .. max-1
inline long random(long howsmall, long howbig) {
  if (howbig <= howsmall) return howsmall;
  const long diff = howbig - howsmall;
  return howsmall + static_cast<long>(mock_arduino::next_random() % static_cast<uint32_t>(diff));
}

// ---------------------------------------------------------------------------
// Serial (in-memory)
// ---------------------------------------------------------------------------
class MockSerial {
public:
  std::deque<char> input;   // bytes the "host" has sent to the device
  std::string output;       // bytes the device has printed

  void begin(unsigned long) {}

  int available() { return static_cast<int>(input.size()); }

  int read() {
    if (input.empty()) return -1;
    const char c = input.front();
    input.pop_front();
    return c;
  }

  void print(const char *s) { output += s; }
  void print(const std::string &s) { output += s; }
  void print(char c) { output += c; }
  void print(int v) { output += std::to_string(v); }

  void println() { output += "\n"; }
  void println(const char *s) { output += s; output += "\n"; }
  void println(const std::string &s) { output += s; output += "\n"; }
  void println(int v) { output += std::to_string(v); output += "\n"; }

  // Test helpers
  void feed(const std::string &s) {
    for (char c : s) input.push_back(c);
  }
  void reset() { input.clear(); output.clear(); }
};

extern MockSerial Serial;

#endif  // ROBOEYES_TEST_MOCK_ARDUINO_H
