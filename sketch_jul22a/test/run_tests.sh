#!/usr/bin/env bash
# Build and run the host-based unit tests for the RoboEyes LCD sketch.
#
# The sketch targets an ESP32 (Deneyap Kart 1A v2), but its drawing,
# compression and animation logic is pure C++ that we compile and test with a
# standard host toolchain using the lightweight Arduino mocks in test/mock.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MOCK="$HERE/mock"
OUT="$HERE/build"
mkdir -p "$OUT"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O0 -g -Wall -Wextra -Wno-unused-parameter}"

# Force-include the mock Arduino core so the sketch's implicit Arduino symbols
# resolve, and put the mock dir first on the include path so <Wire.h>,
# <LiquidCrystal_I2C.h> and <esp_system.h> pick up the mocks.
"$CXX" $CXXFLAGS \
  -I"$MOCK" \
  -include "$MOCK/Arduino.h" \
  "$HERE/test_sketch.cpp" \
  "$MOCK/mock_globals.cpp" \
  -o "$OUT/test_runner"

"$OUT/test_runner"
