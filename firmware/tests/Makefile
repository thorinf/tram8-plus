CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I../src
SRC_DIR = ../src
TEST_DIR = .

MIDI_PARSER_SRC = $(SRC_DIR)/midi_parser.c
UI_SRC = $(SRC_DIR)/ui.c
NOTE_STACK_SRC = $(SRC_DIR)/note_stack.c
LEARN_SRC = $(SRC_DIR)/midi_learn.c $(SRC_DIR)/midi_mapper.c

TESTS = test_midi_parser test_button test_sysex test_note_stack test_learn_modes test_mapper_eeprom

.PHONY: all clean test

all: $(TESTS)

test: all
	@echo "Running tests..."
	@./test_midi_parser
	@./test_button
	@./test_sysex
	@./test_note_stack
	@./test_learn_modes
	@./test_mapper_eeprom
	@echo "All tests completed!"

test_midi_parser: test_midi_parser.c $(MIDI_PARSER_SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_button: test_button.c $(UI_SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_sysex: test_sysex.c
	$(CC) $(CFLAGS) -o $@ $^

test_note_stack: test_note_stack.c $(NOTE_STACK_SRC)
	$(CC) $(CFLAGS) -o $@ $^

test_learn_modes: test_learn_modes.c $(LEARN_SRC) $(MIDI_PARSER_SRC) $(UI_SRC) $(NOTE_STACK_SRC)
	$(CC) $(CFLAGS) -Istubs -o $@ $^

test_mapper_eeprom: test_mapper_eeprom.c $(SRC_DIR)/midi_mapper.c
	$(CC) $(CFLAGS) -Istubs -o $@ $^

clean:
	rm -f $(TESTS)
