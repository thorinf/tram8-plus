CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
SRC_DIR = ../src
TEST_DIR = .

MIDI_PARSER_SRC = $(SRC_DIR)/midi_parser.c
MIDI_MAP_SRC = $(SRC_DIR)/midi_map.c

TESTS = test_midi_parser

.PHONY: all clean test

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_parser
	@echo "All tests completed!"

test_midi_parser: test_midi_parser.c $(MIDI_PARSER_SRC)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -o $@ $^

clean:
	rm -f $(TESTS)
