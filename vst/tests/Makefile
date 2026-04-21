CXX = c++
CXXFLAGS = -Wall -Wextra -std=c++17 -g -I../source

TESTS = test_midi_engine

.PHONY: all clean test

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_engine
	@echo "All tests completed!"

test_midi_engine: test_midi_engine.cpp ../source/midi_engine.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TESTS)
