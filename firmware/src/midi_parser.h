#pragma once

#include <stdint.h>

typedef struct {
  uint8_t status, d1, d2;
  uint8_t len;
} MidiMsg;

typedef struct {
  uint8_t running;
  uint8_t curr;
  uint8_t need, have;
  uint8_t d1_tmp;
  uint8_t desync;
} MidiParser;

void midi_parser_init(MidiParser *p);
uint8_t midi_parse(MidiParser *p, uint8_t byte, MidiMsg *out);
void midi_parser_force_desync(MidiParser *p);
