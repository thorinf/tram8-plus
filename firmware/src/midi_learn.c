#include "midi_learn.h"

#include "gpio.h"
#include "midi_mapper.h"

LearnState g_learn;

void learn_begin(void) {
  g_learn.active = 1;
  g_learn.gate = 0;

  midi_mapper_clear();

  for (uint8_t i = 0; i < NUM_GATES; ++i) {
    gate_set(i, 0);
  }
  gate_set(0, 1);
  led_on();
}

void learn_exit(void) {
  if (!g_learn.active) {
    return;
  }

  g_learn.active = 0;

  for (uint8_t i = 0; i < NUM_GATES; ++i) {
    gate_set(i, 0);
  }
  led_off();
}

uint8_t learn_on_note(uint8_t note) {
  if (!g_learn.active) {
    return 0;
  }

  midi_mapper_set_gate(note, g_learn.gate);
  gate_set(g_learn.gate, 0);
  g_learn.gate++;

  if (g_learn.gate >= NUM_GATES) {
    midi_mapper_save();
    learn_exit();
    return 1;
  }

  gate_set(g_learn.gate, 1);
  return 0;
}

uint8_t learn_get_current_gate(void) {
  return g_learn.gate;
}

uint8_t learn_is_active(void) {
  return g_learn.active;
}
