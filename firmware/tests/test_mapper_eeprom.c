#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware_config.h"
#include "midi_mapper.h"

static uint8_t storage[512];

uint8_t eeprom_read_byte(const uint8_t* address) {
  return storage[(uintptr_t)address];
}

void eeprom_read_block(void* dest, const void* address, size_t size) {
  memcpy(dest, storage + (uintptr_t)address, size);
}

void eeprom_update_byte(uint8_t* address, uint8_t value) {
  storage[(uintptr_t)address] = value;
}

void eeprom_update_block(const void* src, void* address, size_t size) {
  memcpy(storage + (uintptr_t)address, src, size);
}

int main(void) {
  memset(storage, 0xFF, sizeof(storage));
  midi_mapper_init();
  assert(midi_mapper_get_channel() == 9);
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate)
    assert(midi_mapper_get_gates(60 + gate) == (1 << gate));

  // A stock map remains readable without the new marker.
  storage[EEPROM_CHANNEL_ADDR] = 3;
  for (uint8_t gate = 0; gate < NUM_GATES; ++gate)
    storage[EEPROM_NOTEMAP_ADDR + gate] = 80 + gate;
  storage[EEPROM_NOTEMAP_ADDR + 2] = 200;
  storage[EEPROM_NOTEMAP_ADDR + 3] = 80;
  midi_mapper_load();
  assert(midi_mapper_get_channel() == 3);
  assert(midi_mapper_get_gates(60) == 0);
  assert(midi_mapper_get_gates(80) == ((1 << 0) | (1 << 3)));
  assert(midi_mapper_get_note_for_gate(2) == 0xFF);

  // Gate zero unassigned must not turn a saved sparse map into defaults.
  midi_mapper_clear();
  midi_mapper_set_gate(0, 1);
  midi_mapper_set_gate(127, 7);
  midi_mapper_set_channel(15);
  midi_mapper_save();
  assert(storage[EEPROM_NOTEMAP_ADDR] == 0xFF);
  midi_mapper_clear();
  midi_mapper_set_channel(0);
  midi_mapper_load();
  assert(midi_mapper_get_channel() == 15);
  for (uint8_t note = 0; note < 128; ++note)
    assert(midi_mapper_get_gates(note) == (note == 0 ? 2 : note == 127 ? 128 : 0));

  midi_mapper_clear();
  midi_mapper_save();
  midi_mapper_set_gate(60, 0);
  midi_mapper_load();
  for (uint8_t note = 0; note < 128; ++note)
    assert(midi_mapper_get_gates(note) == 0);

  storage[EEPROM_CHANNEL_ADDR] = 255;
  midi_mapper_load();
  assert(midi_mapper_get_channel() == 9);
  puts("mapper EEPROM passed (erased, legacy, invalid notes, sparse and empty maps)");
}
