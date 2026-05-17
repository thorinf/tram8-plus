#ifndef NOTE_STACK_H
#define NOTE_STACK_H

#include <stdint.h>

#define NOTE_STACK_MAX 4

typedef struct {
  uint8_t notes[NOTE_STACK_MAX];
  uint8_t vels[NOTE_STACK_MAX];
  uint8_t count;
} note_stack_t;

void note_stack_clear(note_stack_t* s);
void note_stack_push(note_stack_t* s, uint8_t note, uint8_t vel);
uint8_t note_stack_remove(note_stack_t* s, uint8_t note);
uint8_t note_stack_empty(const note_stack_t* s);
uint8_t note_stack_top_note(const note_stack_t* s);
uint8_t note_stack_top_vel(const note_stack_t* s);

#endif
