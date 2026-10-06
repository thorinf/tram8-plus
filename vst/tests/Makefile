CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -I../source -I../external/vst3sdk

TESTS = test_midi_engine
ifeq ($(shell uname -s),Darwin)
TESTS += test_webview_bridge
endif

.PHONY: all clean test test_webview_bridge

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_engine
ifeq ($(shell uname -s),Darwin)
	@../build/webview-tests/tests/tram8-webview-bridge-test
endif
	@echo "All tests completed!"

test_midi_engine: test_midi_engine.cpp ../source/midi_engine.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_webview_bridge:
	cmake -S .. -B ../build/webview-tests -DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_RUN_VST_VALIDATOR=OFF
	cmake --build ../build/webview-tests --target tram8-webview-bridge-test -j2

clean:
	rm -f $(TESTS)
