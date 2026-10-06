CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -I../source -I../external/vst3sdk

TESTS = test_midi_engine
ifeq ($(shell uname -s),Darwin)
TESTS += test_editor_sync
endif

.PHONY: all clean test test_editor_sync

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_engine
ifeq ($(shell uname -s),Darwin)
	@../build/editor-tests/tests/tram8-editor-sync-test
endif
	@echo "All tests completed!"

test_midi_engine: test_midi_engine.cpp ../source/midi_engine.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

test_editor_sync:
	cmake -S .. -B ../build/editor-tests -DSMTG_CREATE_PLUGIN_LINK=OFF -DSMTG_RUN_VST_VALIDATOR=OFF
	cmake --build ../build/editor-tests --target tram8-editor-sync-test -j2

clean:
	rm -f $(TESTS)
