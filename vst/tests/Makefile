CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -I../source -I../external/vst3sdk

TESTS = test_midi_engine
ifeq ($(shell uname -s),Darwin)
TESTS += test_webview_bridge
endif
NATIVE_TESTS = test_midi_port_processor
NATIVE_BUILD = ../build/native-tests

ifeq ($(shell uname -s),Darwin)
test: test-midi-port
endif

.PHONY: all clean test test_webview_bridge test-native native-sdk test-midi-port

all: $(TESTS)

ifeq ($(shell uname -s),Darwin)
test: test-native
endif

test: all
	@echo "Running tests..."
	@./test_midi_engine
ifeq ($(shell uname -s),Darwin)
	@../build/webview-tests/tests/tram8-webview-bridge-test
endif
	@node test_midi_ports.js
	@echo "All tests completed!"

test_midi_engine: test_midi_engine.cpp ../source/midi_engine.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_webview_bridge:
	cmake -S .. -B ../build/webview-tests -DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_RUN_VST_VALIDATOR=OFF
	cmake --build ../build/webview-tests --target tram8-webview-bridge-test -j2

native-sdk:
	cmake -S .. -B $(NATIVE_BUILD) -DCMAKE_BUILD_TYPE=Release \
		-DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_RUN_VST_VALIDATOR=OFF
	cmake --build $(NATIVE_BUILD) --target sdk sdk_hosting -j2

test-native: $(NATIVE_TESTS)
	@./test_midi_port_processor

test_midi_port_processor: test_midi_port_processor.cpp ../source/processor.cpp ../source/midi_engine.cpp native-sdk
	$(CXX) $(CXXFLAGS) -DRELEASE=1 -o $@ $(filter %.cpp,$^) -L$(NATIVE_BUILD)/lib/Release \
		-lsdk -lsdk_hosting -lsdk_common -lbase -lpluginterfaces -framework CoreFoundation

test-midi-port:
	cmake -S .. -B ../build/tests-build -DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_RUN_VST_VALIDATOR=OFF
	cmake --build ../build/tests-build --target test_midi_port -j2
	../build/tests-build/tests/test_midi_port

clean:
	rm -f $(TESTS) $(NATIVE_TESTS)
