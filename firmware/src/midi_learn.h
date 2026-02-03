#ifndef MIDI_LEARN_H
#define MIDI_LEARN_H

#include <stdint.h>

#include "hardware_config.h"

typedef struct {
  uint8_t active;
  uint8_t gate;
} LearnState;

extern LearnState g_learn;

void learn_begin(void);
void learn_exit(void);
uint8_t learn_on_note(uint8_t note);
uint8_t learn_get_current_gate(void);
uint8_t learn_is_active(void);

#endif
