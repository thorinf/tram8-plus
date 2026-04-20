#ifndef MIDI_MAPPER_H
#define MIDI_MAPPER_H

#include <stdint.h>

#include "hardware_config.h"

// Initialize mapper (load from EEPROM)
void midi_mapper_init(void);

// Get gate bitmask for a note (O(1) lookup)
uint8_t midi_mapper_get_gates(uint8_t note);

// Set a gate for a note (used during learn mode)
void midi_mapper_set_gate(uint8_t note, uint8_t gate);

// Clear all mappings
void midi_mapper_clear(void);

// Load mappings from EEPROM
void midi_mapper_load(void);

// Save mappings to EEPROM
void midi_mapper_save(void);

// Get/set MIDI channel
uint8_t midi_mapper_get_channel(void);
void midi_mapper_set_channel(uint8_t channel);

// Get note assigned to a gate (for save compatibility)
uint8_t midi_mapper_get_note_for_gate(uint8_t gate);

#endif
