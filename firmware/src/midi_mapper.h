#ifndef MIDI_MAPPER_H
#define MIDI_MAPPER_H

#include <stdint.h>

#include "hardware_config.h" // NUM_GATES

#define MAPPER_ANY 0xFF

typedef enum { MM_VELOCITY = 0, MM_CC, MM_PITCH, MM_PITCH_SAH, MM_RANDSEQ, MM_RANDSEQ_SAH } MidiMapperMode;

typedef struct {
  uint8_t ch;
  uint8_t note;
} NoteSpec;

typedef struct {
  uint8_t ch;
  uint8_t cc;
} CCSpec;

typedef struct {
  uint8_t mode;
  NoteSpec gate;
  union {
    CCSpec cc;
    uint8_t pitch_ch;
    uint8_t sample_ch;
    struct {
      NoteSpec step;
      NoteSpec reset;
    } rand;
  } secondary;
} MidiMapperEntry;

typedef struct {
  MidiMapperEntry slot[NUM_GATES];
} MidiMapper;

typedef struct {
  uint8_t gate_chan_mask[16];
  uint8_t gate_data1_mask[128];
  uint8_t secondary_note_mask;
  uint8_t secondary_cc_mask;
  uint8_t secondary_chan_mask[16];
  uint8_t secondary_data1_mask[128];
} MidiMapperMasks;

extern const MidiMapperEntry midi_mapper_velo[8];
extern const MidiMapperEntry midi_mapper_cc[8];
extern const MidiMapperEntry midi_mapper_bsp[8];

void midi_mapper_rebuild_masks(void);
void midi_mapper_set_masks(MidiMapperMasks *storage);
uint8_t midi_mapper_gate_mask(uint8_t channel, uint8_t note);
uint8_t midi_mapper_secondary_mask(uint8_t channel, uint8_t control);
MidiMapper *midi_mapper_get_map(void);

#endif
