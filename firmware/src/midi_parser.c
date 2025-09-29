#include "midi_parser.h"
#include <stdint.h>

static inline uint8_t data_need(uint8_t s) {
  uint8_t hi = s & 0xF0;
  return (hi == 0xC0 || hi == 0xD0) ? 1 : 2; // Program Change and Pressure Value only 1 data byte, else 2
}

void midi_parser_init(MidiParser *p) { p->running = p->curr = p->need = p->have = p->d1_tmp = p->desync = 0; }

void midi_parser_force_desync(MidiParser *p) {
  p->running = 0;
  p->curr = 0;
  p->need = 0;
  p->have = 0;
  p->d1_tmp = 0;
  p->desync = 1;
}

uint8_t midi_parse(MidiParser *p, uint8_t b, MidiMsg *out) {
  if (b >= 0xF8) {
    // system real-time messages always passes
    // desync doesn't matter as they're length 1
    out->status = b;
    out->d1 = out->d2 = 0;
    out->len = 1;
    return 1;
  }

  if (p->desync) {
    if (b < 0x80) return 0; // ignore data until status
    p->desync = 0;          // got status, continue below
  }

  if (b & 0x80) {    // status
    if (b >= 0xF0) { // ignore SysEx/System Common
      p->running = p->curr = p->need = p->have = 0;
      return 0;
    }
    p->running = p->curr = b;
    p->need = data_need(p->curr);
    p->have = 0;
    return 0;
  }
  // data
  if (p->curr == 0) {
    if (!p->running) return 0; // stray data
    p->curr = p->running;
    p->need = data_need(p->curr);
    p->have = 0;
  }
  if (p->need == 1) {
    out->status = p->curr;
    out->d1 = b;
    out->d2 = 0;
    out->len = 2;
    p->curr = 0;
    return 1;
  }
  if (p->have == 0) {
    p->d1_tmp = b;
    p->have = 1;
    return 0;
  }
  out->status = p->curr;
  out->d1 = p->d1_tmp;
  out->d2 = b;
  out->len = 3;
  p->curr = 0;
  p->have = 0;
  return 1;
}
