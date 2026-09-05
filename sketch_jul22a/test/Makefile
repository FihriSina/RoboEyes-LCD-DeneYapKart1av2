# Host-based unit tests for the RoboEyes LCD sketch.
#
#   make        # build and run the tests
#   make run    # same
#   make coverage  # build with gcov and report line coverage of the sketch
#   make clean

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O0 -g -Wall -Wextra -Wno-unused-parameter
MOCK      = mock
BUILD     = build

INCLUDES  = -I$(MOCK) -include $(MOCK)/Arduino.h
SOURCES   = test_sketch.cpp $(MOCK)/mock_globals.cpp

.PHONY: all run coverage clean

all: run

run: $(BUILD)/test_runner
	./$(BUILD)/test_runner

$(BUILD)/test_runner: test_sketch.cpp $(MOCK)/mock_globals.cpp $(MOCK)/*.h test_framework.h ../sketch_jul22a.ino
	@mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $@

coverage:
	@mkdir -p $(BUILD)/cov
	$(CXX) $(CXXFLAGS) --coverage $(INCLUDES) $(SOURCES) -o $(BUILD)/cov/cov_runner
	@cd $(BUILD)/cov && ./cov_runner >/dev/null
	@cd $(BUILD)/cov && gcov cov_runner-test_sketch.gcda 2>/dev/null | grep -A1 "sketch_jul22a.ino"

clean:
	rm -rf $(BUILD)
