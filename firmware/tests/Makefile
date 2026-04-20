CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I../src
SRC_DIR = ../src
TEST_DIR = .

MIDI_PARSER_SRC = $(SRC_DIR)/midi_parser.c
UI_SRC = $(SRC_DIR)/ui.c

TESTS = test_midi_parser test_button

.PHONY: all clean test

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_parser
	@./test_button
	@echo "All tests completed!"

test_midi_parser: test_midi_parser.c $(MIDI_PARSER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_button: test_button.c $(UI_SRC)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TESTS)
