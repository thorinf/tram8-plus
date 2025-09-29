#include "../src/midi_parser.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_MESSAGES 16

typedef struct {
  MidiMsg messages[MAX_MESSAGES];
  uint8_t count;
} MidiMsgVector;

MidiMsgVector process_bytes(const uint8_t *bytes, uint8_t byte_count) {
  MidiParser parser;
  MidiMsgVector result = (MidiMsgVector){0};
  MidiMsg msg;

  midi_parser_init(&parser);

  for (uint8_t i = 0; i < byte_count && result.count < MAX_MESSAGES; i++) {
    if (midi_parse(&parser, bytes[i], &msg)) { result.messages[result.count++] = msg; }
  }

  return result;
}

static void assert_msg_equals(const MidiMsg *expected, const MidiMsg *actual, const char *test_name) {
  assert(actual->status == expected->status);
  assert(actual->d1 == expected->d1);
  assert(actual->d2 == expected->d2);
  assert(actual->len == expected->len);
  printf("%s passed\n", test_name);
}

static void test_running_status() {
  uint8_t bytes[] = {0x90, 60, 64, 0x90, 62, 32};
  MidiMsgVector result = process_bytes(bytes, 6);

  assert(result.count == 2);

  MidiMsg expected1 = {0x90, 60, 64, 3};
  MidiMsg expected2 = {0x90, 62, 32, 3};

  assert_msg_equals(&expected1, &result.messages[0], "Running status first message");
  assert_msg_equals(&expected2, &result.messages[1], "Running status second message");
}

static void test_program_change() {
  uint8_t bytes[] = {0xC1, 0x05};
  MidiMsgVector result = process_bytes(bytes, 2);

  assert(result.count == 1);

  MidiMsg expected = {0xC1, 0x05, 0x00, 2};
  assert_msg_equals(&expected, &result.messages[0], "Program change single data byte");
}

static void test_realtime_passthrough() {
  uint8_t bytes[] = {0xF8, 0x90, 60, 0x40};
  MidiMsgVector result = process_bytes(bytes, 4);

  assert(result.count == 2);

  MidiMsg realtime = {0xF8, 0x00, 0x00, 1};
  MidiMsg note_on = {0x90, 60, 0x40, 3};

  assert_msg_equals(&realtime, &result.messages[0], "Real-time message always emitted");
  assert_msg_equals(&note_on, &result.messages[1], "Note-on after real-time");
}

static void test_stray_data_then_status() {
  uint8_t bytes[] = {0x45, 0x46, 0x90, 0x30, 0x10};
  MidiMsgVector result = process_bytes(bytes, 5);

  assert(result.count == 1);

  MidiMsg expected = {0x90, 0x30, 0x10, 3};
  assert_msg_equals(&expected, &result.messages[0], "Stray data ignored until status");
}

static void test_desync_recovery() {
  MidiParser parser;
  MidiMsg msg;

  midi_parser_init(&parser);

  assert(midi_parse(&parser, 0x90, &msg) == 0); // start running status
  midi_parser_force_desync(&parser);

  assert(midi_parse(&parser, 0x40, &msg) == 0); // ignored until new status
  assert(midi_parse(&parser, 0x80, &msg) == 0);
  assert(midi_parse(&parser, 0x40, &msg) == 0);
  assert(midi_parse(&parser, 0x00, &msg) == 1);

  MidiMsg expected = {0x80, 0x40, 0x00, 3};
  assert_msg_equals(&expected, &msg, "Desync recovers on new status");
}

int main() {
  printf("Running MIDI Parser tests...\n");

  test_running_status();
  test_program_change();
  test_realtime_passthrough();
  test_stray_data_then_status();
  test_desync_recovery();

  printf("All MIDI Parser tests passed!\n");
  return 0;
}
