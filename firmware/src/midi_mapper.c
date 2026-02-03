#include "midi_mapper.h"

#include <avr/eeprom.h>
#include <string.h>

#include "hardware_config.h"

// 128-byte lookup: note -> gate bitmask. Stock EEPROM stores 8 notes at 0x101.
static uint8_t note_to_gates[128];
static uint8_t midi_channel = 9;
static const uint8_t default_notes[NUM_GATES] = {60, 61, 62, 63, 64, 65, 66, 67};

void midi_mapper_init(void) { midi_mapper_load(); }

uint8_t midi_mapper_get_gates(uint8_t note) { return note_to_gates[note & 0x7F]; }

void midi_mapper_set_gate(uint8_t note, uint8_t gate) {
  if (gate < NUM_GATES) { note_to_gates[note & 0x7F] |= (uint8_t)(1 << gate); }
}

void midi_mapper_clear(void) { memset(note_to_gates, 0, sizeof(note_to_gates)); }

void midi_mapper_load(void) {
  midi_mapper_clear();

  midi_channel = eeprom_read_byte((uint8_t *)EEPROM_CHANNEL_ADDR);
  if (midi_channel > 15) { midi_channel = 9; }

  uint8_t notes[NUM_GATES];
  eeprom_read_block(notes, (uint8_t *)EEPROM_NOTEMAP_ADDR, NUM_GATES);

  if (notes[0] == 0xFF) {
    for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
      note_to_gates[default_notes[gate]] |= (uint8_t)(1 << gate);
    }
  } else {
    for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
      if (notes[gate] < 128) { note_to_gates[notes[gate]] |= (uint8_t)(1 << gate); }
    }
  }
}

void midi_mapper_save(void) {
  eeprom_update_byte((uint8_t *)EEPROM_CHANNEL_ADDR, midi_channel);

  uint8_t notes[NUM_GATES];
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate) {
    notes[gate] = midi_mapper_get_note_for_gate(gate);
  }

  eeprom_update_block(notes, (uint8_t *)EEPROM_NOTEMAP_ADDR, NUM_GATES);
}

uint8_t midi_mapper_get_channel(void) { return midi_channel; }

void midi_mapper_set_channel(uint8_t channel) { midi_channel = channel & 0x0F; }

uint8_t midi_mapper_get_note_for_gate(uint8_t gate) {
  if (gate >= NUM_GATES) { return 0xFF; }

  uint8_t mask = (uint8_t)(1 << gate);
  for (uint8_t note = 0; note < 128; ++note) {
    if (note_to_gates[note] & mask) { return note; }
  }

  return 0xFF;
}
