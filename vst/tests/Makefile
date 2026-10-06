CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -I../source -I../external/vst3sdk

TESTS = test_midi_engine

ifeq ($(shell uname -s),Darwin)
test: test-midi-port
endif

.PHONY: all clean test test-midi-port

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_engine
	@echo "All tests completed!"

test_midi_engine: test_midi_engine.cpp ../source/midi_engine.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test-midi-port:
	cmake -S .. -B ../build/tests-build -DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_RUN_VST_VALIDATOR=OFF
	cmake --build ../build/tests-build --target test_midi_port -j2
	../build/tests-build/tests/test_midi_port

clean:
	rm -f $(TESTS)
