#include "midi_mapper.h"

#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#endif

#include <stdint.h>
#include <string.h>

#include "hardware_config.h"

static uint8_t gate_chan_mask[16] = {0};
static uint8_t gate_data1_mask[128] = {0};

static uint8_t secondary_note_mask = 0;
static uint8_t secondary_cc_mask = 0;
static uint8_t secondary_chan_mask[16] = {0};
static uint8_t secondary_data1_mask[128] = {0};

static MidiMapper g_map;

const MidiMapperEntry midi_mapper_velo[8] PROGMEM = {
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 24}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 25}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 26}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 27}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 28}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 29}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 30}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 0, .note = 31}, .secondary = {0}},
};

const MidiMapperEntry midi_mapper_cc[8] PROGMEM = {
    {.mode = MM_CC, .gate = {.ch = 0, .note = 24}, .secondary = {.cc = {.ch = 0, .cc = 69}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 25}, .secondary = {.cc = {.ch = 0, .cc = 70}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 26}, .secondary = {.cc = {.ch = 0, .cc = 71}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 27}, .secondary = {.cc = {.ch = 0, .cc = 72}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 28}, .secondary = {.cc = {.ch = 0, .cc = 73}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 29}, .secondary = {.cc = {.ch = 0, .cc = 74}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 30}, .secondary = {.cc = {.ch = 0, .cc = 75}}},
    {.mode = MM_CC, .gate = {.ch = 0, .note = 31}, .secondary = {.cc = {.ch = 0, .cc = 76}}},
};

const MidiMapperEntry midi_mapper_bsp[8] PROGMEM = {
    {.mode = MM_RANDSEQ_SAH, .gate = {.ch = 7, .note = 36}, .secondary = {0}},
    {.mode = MM_RANDSEQ_SAH, .gate = {.ch = 7, .note = 37}, .secondary = {0}},
    {.mode = MM_RANDSEQ, .gate = {.ch = 7, .note = 38}, .secondary = {0}},
    {.mode = MM_RANDSEQ, .gate = {.ch = 7, .note = 39}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 7, .note = 40}, .secondary = {0}},
    {.mode = MM_VELOCITY, .gate = {.ch = 7, .note = 41}, .secondary = {0}},
    {.mode = MM_PITCH, .gate = {.ch = 0, .note = MAPPER_ANY}, .secondary = {0}},
    {.mode = MM_PITCH, .gate = {.ch = 1, .note = MAPPER_ANY}, .secondary = {0}},
};

static void set_bit_all(uint8_t *buffer, uint8_t length, uint8_t bit) {
  for (uint8_t i = 0; i < length; ++i) {
    buffer[i] |= bit;
  }
}

static void add_gate_mapping(uint8_t channel, uint8_t data1, uint8_t bit) {
  if (channel == MAPPER_ANY) {
    set_bit_all(gate_chan_mask, 16, bit);
  } else {
    gate_chan_mask[channel & 0x0F] |= bit;
  }

  if (data1 == MAPPER_ANY) {
    set_bit_all(gate_data1_mask, 128, bit);
  } else {
    gate_data1_mask[data1 & 0x7F] |= bit;
  }
}

static void add_secondary_mapping(uint8_t channel, uint8_t data1, uint8_t bit) {
  if (channel == MAPPER_ANY) {
    set_bit_all(secondary_chan_mask, 16, bit);
  } else {
    secondary_chan_mask[channel & 0x0F] |= bit;
  }

  if (data1 == MAPPER_ANY) {
    set_bit_all(secondary_data1_mask, 128, bit);
  } else {
    secondary_data1_mask[data1 & 0x7F] |= bit;
  }
}

static void add_secondary_note_mapping(uint8_t channel, uint8_t note, uint8_t bit) {
  secondary_note_mask |= bit;
  add_secondary_mapping(channel, note, bit);
}

static void add_secondary_cc_mapping(uint8_t channel, uint8_t control, uint8_t bit) {
  secondary_cc_mask |= bit;
  add_secondary_mapping(channel, control, bit);
}

static void populate_masks_for_gate(uint8_t gate_index, const MidiMapperEntry *entry) {
  uint8_t bit = (uint8_t)(1u << gate_index);

  switch (entry->mode) {
  case MM_VELOCITY:
    add_gate_mapping(entry->gate.ch, entry->gate.note, bit);
    break;
  case MM_CC:
    add_gate_mapping(entry->gate.ch, entry->gate.note, bit);
    add_secondary_cc_mapping(entry->secondary.cc.ch, entry->secondary.cc.cc, bit);
    break;
  case MM_PITCH:
    add_gate_mapping(entry->gate.ch, MAPPER_ANY, bit);
    break;
  case MM_PITCH_SAH:
    add_gate_mapping(entry->gate.ch, entry->gate.note, bit);
    add_secondary_note_mapping(entry->secondary.sample_ch, MAPPER_ANY, bit);
    break;
  case MM_RANDSEQ:
    add_gate_mapping(entry->gate.ch, entry->gate.note, bit);
    add_secondary_note_mapping(entry->secondary.rand.step.ch, entry->secondary.rand.step.note, bit);
    add_secondary_note_mapping(entry->secondary.rand.reset.ch, entry->secondary.rand.reset.note, bit);
    break;
  case MM_RANDSEQ_SAH:
    add_gate_mapping(entry->gate.ch, entry->gate.note, bit);
    add_secondary_note_mapping(entry->secondary.rand.step.ch, entry->secondary.rand.step.note, bit);
    add_secondary_note_mapping(entry->secondary.rand.reset.ch, entry->secondary.rand.reset.note, bit);
    break;
  default:
    break;
  }
}

void midi_mapper_rebuild_masks(void) {
  memset(gate_chan_mask, 0, sizeof gate_chan_mask);
  memset(gate_data1_mask, 0, sizeof gate_data1_mask);
  secondary_note_mask = 0;
  secondary_cc_mask = 0;
  memset(secondary_chan_mask, 0, sizeof secondary_chan_mask);
  memset(secondary_data1_mask, 0, sizeof secondary_data1_mask);

  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    populate_masks_for_gate(gate, &g_map.slot[gate]);
  }
}

uint8_t midi_mapper_gate_mask(uint8_t channel, uint8_t note) {
  const uint8_t chan_bits = gate_chan_mask[channel & 0x0F];
  const uint8_t note_bits = gate_data1_mask[note & 0x7F];
  return (uint8_t)(chan_bits & note_bits);
}

uint8_t midi_mapper_secondary_mask(uint8_t channel, uint8_t control) {
  const uint8_t chan_bits = secondary_chan_mask[channel & 0x0F];
  const uint8_t ctrl_bits = secondary_data1_mask[control & 0x7F];
  return (uint8_t)((chan_bits & ctrl_bits));
}

MidiMapper *midi_mapper_get_map(void) { return &g_map; }
