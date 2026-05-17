#include "note_stack.h"

void note_stack_clear(note_stack_t* s) {
  s->count = 0;
}

void note_stack_push(note_stack_t* s, uint8_t note, uint8_t vel) {
  // De-dup: remove existing entry for same note first
  note_stack_remove(s, note);
  if (s->count < NOTE_STACK_MAX) {
    s->notes[s->count] = note;
    s->vels[s->count] = vel;
    s->count++;
  } else {
    // Stack full: drop oldest, shift up, append new on top
    for (uint8_t i = 1; i < NOTE_STACK_MAX; ++i) {
      s->notes[i - 1] = s->notes[i];
      s->vels[i - 1] = s->vels[i];
    }
    s->notes[NOTE_STACK_MAX - 1] = note;
    s->vels[NOTE_STACK_MAX - 1] = vel;
  }
}

uint8_t note_stack_remove(note_stack_t* s, uint8_t note) {
  for (uint8_t i = 0; i < s->count; ++i) {
    if (s->notes[i] == note) {
      for (uint8_t j = i; j + 1 < s->count; ++j) {
        s->notes[j] = s->notes[j + 1];
        s->vels[j] = s->vels[j + 1];
      }
      s->count--;
      return 1;
    }
  }
  return 0;
}

uint8_t note_stack_empty(const note_stack_t* s) {
  return s->count == 0;
}

uint8_t note_stack_top_note(const note_stack_t* s) {
  if (s->count == 0)
    return 0;
  return s->notes[s->count - 1];
}

uint8_t note_stack_top_vel(const note_stack_t* s) {
  if (s->count == 0)
    return 0;
  return s->vels[s->count - 1];
}
