#include "../source/midi_engine.h"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace tram8;

static void test_note_stack_push_pop() {
  NoteStack stack;
  assert(stack.empty());

  stack.push(0, 60, 100);
  assert(!stack.empty());
  assert(stack.top().note == 60);
  assert(stack.top().velocity == 100);

  stack.push(0, 64, 80);
  assert(stack.top().note == 64);
  assert(stack.top().velocity == 80);

  stack.remove(0, 64);
  assert(stack.top().note == 60);

  stack.remove(0, 60);
  assert(stack.empty());

  printf("note_stack_push_pop passed\n");
}

static void test_note_stack_retrigger() {
  NoteStack stack;
  stack.push(0, 60, 100);
  stack.push(0, 64, 80);
  stack.push(0, 60, 90);

  assert(stack.top().note == 60);
  assert(stack.top().velocity == 90);
  assert(stack.count == 2);

  printf("note_stack_retrigger passed\n");
}

static void test_note_stack_overflow() {
  NoteStack stack;
  for (int i = 0; i < NoteStack::kMaxNotes + 4; i++)
    stack.push(0, i, 64);

  assert(stack.count == NoteStack::kMaxNotes);
  assert(stack.top().note == NoteStack::kMaxNotes - 1);

  printf("note_stack_overflow passed\n");
}

static void test_note_stack_channel_isolation() {
  NoteStack stack;
  stack.push(0, 60, 100);
  stack.push(1, 60, 80);
  assert(stack.count == 2);

  stack.remove(0, 60);
  assert(stack.count == 1);
  assert(stack.top().channel == 1);
  assert(stack.top().note == 60);

  printf("note_stack_channel_isolation passed\n");
}

static void test_velocity_mode() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] == 127 << 7);

  engine.noteOn(0, 60, 0.5f);
  uint16_t half = (uint16_t)(0.5f * 127.0f) << 7;
  assert(engine.dacValues()[0] == half);

  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  printf("velocity_mode passed\n");
}

static void test_velocity_zero_as_note_off() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  engine.noteOn(0, 60, 0.0f);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  printf("velocity_zero_as_note_off passed\n");
}

static void test_pitch_mode() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 0, 0.5f);
  uint16_t val0 = engine.dacValues()[0];

  engine.noteOff(0, 0);
  engine.noteOn(0, 30, 0.5f);
  uint16_t val30 = engine.dacValues()[0];

  engine.noteOff(0, 30);
  engine.noteOn(0, 60, 0.5f);
  uint16_t val60 = engine.dacValues()[0];

  assert(val0 < val30);
  assert(val30 < val60);

  printf("pitch_mode passed\n");
}

static void test_pitch_hold_on_note_off() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 0.8f);
  uint16_t pitchVal = engine.dacValues()[0];
  assert(pitchVal > 0);

  engine.noteOff(0, 48);
  assert(engine.dacValues()[0] == pitchVal);

  printf("pitch_hold_on_note_off passed\n");
}

static void test_last_note_priority() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 0.8f);
  uint16_t pitch48 = engine.dacValues()[0];

  engine.noteOn(0, 60, 0.8f);
  uint16_t pitch60 = engine.dacValues()[0];
  assert(pitch60 > pitch48);

  engine.noteOff(0, 60);
  assert(engine.dacValues()[0] == pitch48);

  engine.noteOff(0, 48);
  assert(engine.dacValues()[0] == pitch48);

  printf("last_note_priority passed\n");
}

static void test_cc_mode() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacCC);
  engine.setDacChannel(0, -1);
  engine.setCcNum(0, 1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.dacValues()[0] == 0);

  engine.setCcValue(1, 100);
  assert(engine.dacValues()[0] == (uint16_t)100 << 7);

  engine.setCcValue(1, 0);
  assert(engine.dacValues()[0] == 0);

  printf("cc_mode passed\n");
}

static void test_gate_note_filter() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 61, 0.8f);
  assert(!(engine.gateMask() & 1));

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  printf("gate_note_filter passed\n");
}

static void test_gate_channel_filter() {
  MidiEngine engine;
  engine.setGateChannel(0, 0);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(1, 60, 0.8f);
  assert(!(engine.gateMask() & 1));

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  printf("gate_channel_filter passed\n");
}

static void test_dac_independence() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 72, 0.8f);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] > 0);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  printf("dac_independence passed\n");
}

static void test_state_changed() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.markSent();
  assert(!engine.stateChanged());

  engine.noteOn(0, 60, 0.8f);
  assert(engine.stateChanged());

  engine.markSent();
  assert(!engine.stateChanged());

  printf("state_changed passed\n");
}

static void test_dac_changed() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.markSent();
  assert(!engine.dacChanged());

  engine.noteOn(0, 48, 0.8f);
  assert(engine.dacChanged());

  engine.markSent();
  assert(!engine.dacChanged());

  printf("dac_changed passed\n");
}

static void test_has_pitch_mode() {
  MidiEngine engine;
  assert(!engine.hasPitchMode());

  engine.setDacMode(0, kDacPitch);
  assert(engine.hasPitchMode());

  engine.setDacMode(0, kDacVelocity);
  assert(!engine.hasPitchMode());

  printf("has_pitch_mode passed\n");
}

static void test_serialize_deserialize() {
  MidiEngine engine;
  engine.setGateChannel(0, 5);
  engine.setGateNote(0, 72);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, 3);
  engine.setCcNum(0, 42);

  int32_t buf[kNumGates * MidiEngine::kStateWordsPerGate];
  engine.serialize(buf);

  MidiEngine engine2;
  engine2.deserialize(buf);

  int32_t buf2[kNumGates * MidiEngine::kStateWordsPerGate];
  engine2.serialize(buf2);

  assert(memcmp(buf, buf2, sizeof(buf)) == 0);

  printf("serialize_deserialize passed\n");
}

static void test_reset() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() != 0);

  engine.reset();
  assert(engine.gateMask() == 0);
  assert(engine.dacValues()[0] == 0);
  assert(!engine.stateChanged());

  printf("reset passed\n");
}

static void test_multi_gate() {
  MidiEngine engine;
  for (int g = 0; g < kNumGates; g++) {
    engine.setGateChannel(g, -1);
    engine.setGateNote(g, 60 + g);
    engine.setDacMode(g, kDacVelocity);
    engine.setDacChannel(g, -1);
  }

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() == 0x01);

  engine.noteOn(0, 63, 0.8f);
  assert(engine.gateMask() == 0x09);

  engine.noteOff(0, 60);
  assert(engine.gateMask() == 0x08);

  printf("multi_gate passed\n");
}

static void test_dac_off_mode() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacOff);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.dacValues()[0] == 0);

  printf("dac_off_mode passed\n");
}

static void test_runtime_mode_change() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 1.0f);
  assert(engine.dacValues()[0] == 127 << 7);

  engine.setDacMode(0, kDacPitch);
  engine.noteOn(0, 48, 1.0f);
  uint16_t pitchVal = engine.dacValues()[0];
  assert(pitchVal != 127 << 7);
  assert(pitchVal > 0);

  printf("runtime_mode_change passed\n");
}

static void test_config_change_gate_channel() {
  MidiEngine engine;
  engine.setGateChannel(0, 0);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(1, 60, 0.8f);
  assert(!(engine.gateMask() & 1));
  engine.noteOff(1, 60);

  engine.setGateChannel(0, 1);

  engine.noteOn(1, 60, 0.8f);
  assert(engine.gateMask() & 1);
  engine.noteOff(1, 60);

  engine.noteOn(0, 60, 0.8f);
  assert(!(engine.gateMask() & 1));

  printf("config_change_gate_channel passed\n");
}

static void test_config_change_gate_note() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);
  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));

  engine.setGateNote(0, 72);

  engine.noteOn(0, 60, 0.8f);
  assert(!(engine.gateMask() & 1));

  engine.noteOn(0, 72, 0.8f);
  assert(engine.gateMask() & 1);

  printf("config_change_gate_note passed\n");
}

static void test_config_change_dac_mode_velocity_to_pitch() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 1.0f);
  uint16_t velVal = engine.dacValues()[0];
  assert(velVal == 127 << 7);

  engine.setDacMode(0, kDacPitch);

  engine.noteOn(0, 48, 1.0f);
  uint16_t pitchVal = engine.dacValues()[0];
  assert(pitchVal != velVal);
  assert(pitchVal > 0);

  printf("config_change_dac_mode_velocity_to_pitch passed\n");
}

static void test_config_change_dac_mode_to_cc() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);
  engine.setCcNum(0, 7);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.dacValues()[0] == 127 << 7);

  engine.setDacMode(0, kDacCC);
  engine.setCcValue(7, 64);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.dacValues()[0] == (uint16_t)64 << 7);

  printf("config_change_dac_mode_to_cc passed\n");
}

static void test_config_change_cc_num() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacCC);
  engine.setDacChannel(0, -1);
  engine.setCcNum(0, 1);

  engine.noteOn(0, 60, 0.8f);
  engine.setCcValue(1, 100);
  assert(engine.dacValues()[0] == (uint16_t)100 << 7);

  engine.setCcNum(0, 7);
  engine.setCcValue(7, 50);

  assert(engine.dacValues()[0] == (uint16_t)50 << 7);

  engine.setCcValue(1, 127);
  assert(engine.dacValues()[0] == (uint16_t)50 << 7);

  printf("config_change_cc_num passed\n");
}

static void test_config_change_dac_channel() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, 0);

  engine.noteOn(0, 48, 0.8f);
  uint16_t pitch48 = engine.dacValues()[0];
  assert(pitch48 > 0);

  engine.noteOn(1, 60, 0.8f);
  assert(engine.dacValues()[0] == pitch48);

  engine.setDacChannel(0, 1);

  engine.noteOn(1, 60, 0.8f);
  uint16_t pitch60 = engine.dacValues()[0];
  assert(pitch60 > pitch48);

  printf("config_change_dac_channel passed\n");
}

static void test_config_sequence_full_workflow() {
  MidiEngine engine;
  for (int g = 0; g < kNumGates; g++) {
    engine.setGateChannel(g, -1);
    engine.setGateNote(g, 60 + g);
    engine.setDacMode(g, kDacVelocity);
    engine.setDacChannel(g, -1);
  }

  engine.noteOn(0, 60, 0.5f);
  assert(engine.gateMask() == 0x01);
  uint16_t vel0 = engine.dacValues()[0];
  assert(vel0 > 0);

  engine.noteOn(0, 61, 0.8f);
  assert(engine.gateMask() == 0x03);
  uint16_t vel1 = engine.dacValues()[1];
  assert(vel1 > vel0);

  engine.setDacMode(0, kDacPitch);

  engine.noteOff(0, 60);
  engine.noteOn(0, 60, 0.3f);
  assert(engine.gateMask() & 1);
  uint16_t pitch0 = engine.dacValues()[0];
  assert(pitch0 != vel0);

  engine.setGateNote(0, -1);
  engine.noteOff(0, 60);

  engine.noteOn(0, 72, 0.8f);
  assert(engine.gateMask() & 1);

  engine.setDacMode(0, kDacCC);
  engine.setCcNum(0, 11);
  engine.setCcValue(11, 80);
  engine.noteOn(0, 72, 0.8f);
  assert(engine.dacValues()[0] == (uint16_t)80 << 7);

  printf("config_sequence_full_workflow passed\n");
}

static void test_gate_held_with_overlapping_notes() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  engine.noteOn(0, 62, 0.8f);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 60);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 62);
  assert(!(engine.gateMask() & 1));

  printf("gate_held_with_overlapping_notes passed\n");
}

static void test_gate_held_release_in_any_order() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  engine.noteOn(0, 64, 0.8f);
  engine.noteOn(0, 67, 0.8f);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 64);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 67);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));

  printf("gate_held_release_in_any_order passed\n");
}

static void test_gate_specific_note_overlapping() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  engine.noteOn(0, 60, 0.9f);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));

  printf("gate_specific_note_overlapping passed\n");
}

static void test_cross_channel_note_independence() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 0.8f);
  uint16_t pitch48 = engine.dacValues()[0];
  assert(engine.gateMask() & 1);

  engine.noteOn(1, 48, 0.8f);
  assert(engine.gateMask() & 1);

  engine.noteOff(0, 48);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] == pitch48);

  engine.noteOff(1, 48);
  assert(!(engine.gateMask() & 1));

  printf("cross_channel_note_independence passed\n");
}

static void test_cc_dac_independent_of_gate() {
  MidiEngine engine;
  engine.setGateChannel(0, 0);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacCC);
  engine.setDacChannel(0, 1);
  engine.setCcNum(0, 7);

  engine.noteOn(1, 64, 0.8f);
  assert(!(engine.gateMask() & 1));

  engine.setCcValue(7, 64);
  assert(engine.dacValues()[0] == (uint16_t)64 << 7);

  engine.setCcValue(7, 0);
  assert(engine.dacValues()[0] == 0);

  printf("cc_dac_independent_of_gate passed\n");
}

static void test_velocity_cross_channel_fallback() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.25f);
  uint16_t ch0Vel = (uint16_t)(0.25f * 127.0f) << 7;
  assert(engine.dacValues()[0] == ch0Vel);

  engine.noteOn(1, 60, 0.75f);
  uint16_t ch1Vel = (uint16_t)(0.75f * 127.0f) << 7;
  assert(engine.dacValues()[0] == ch1Vel);

  engine.noteOff(1, 60);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] == ch0Vel);

  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  printf("velocity_cross_channel_fallback passed\n");
}

static void test_gate_channel_change_clears_gate() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  engine.setGateChannel(0, 1);
  assert(!(engine.gateMask() & 1));

  engine.noteOn(1, 64, 0.8f);
  assert(engine.gateMask() & 1);

  printf("gate_channel_change_clears_gate passed\n");
}

static void test_gate_note_change_clears_gate() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);

  engine.setGateNote(0, 72);
  assert(!(engine.gateMask() & 1));

  printf("gate_note_change_clears_gate passed\n");
}

static void test_dac_channel_change_clears_stack() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 0.8f);
  engine.noteOn(0, 60, 0.8f);
  uint16_t pitch60 = engine.dacValues()[0];

  engine.setDacChannel(0, 1);

  engine.noteOn(1, 36, 0.8f);
  uint16_t pitch36 = engine.dacValues()[0];
  assert(pitch36 < pitch60);

  engine.noteOff(1, 36);
  assert(engine.dacValues()[0] == pitch36);

  printf("dac_channel_change_clears_stack passed\n");
}

static void test_dac_mode_change_clears_value() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.dacValues()[0] == 127 << 7);

  engine.setDacMode(0, kDacOff);
  assert(engine.dacValues()[0] == 0);

  printf("dac_mode_change_clears_value passed\n");
}

static void test_deserialize_clears_runtime() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] > 0);

  int32_t buf[kNumGates * MidiEngine::kStateWordsPerGate];
  MidiEngine defaults;
  defaults.serialize(buf);

  engine.deserialize(buf);
  assert(engine.gateMask() == 0);
  assert(engine.dacValues()[0] == 0);

  printf("deserialize_clears_runtime passed\n");
}

int main() {
  test_note_stack_push_pop();
  test_note_stack_retrigger();
  test_note_stack_overflow();
  test_note_stack_channel_isolation();
  test_velocity_mode();
  test_velocity_zero_as_note_off();
  test_pitch_mode();
  test_pitch_hold_on_note_off();
  test_last_note_priority();
  test_cc_mode();
  test_gate_note_filter();
  test_gate_channel_filter();
  test_dac_independence();
  test_state_changed();
  test_dac_changed();
  test_has_pitch_mode();
  test_serialize_deserialize();
  test_reset();
  test_multi_gate();
  test_dac_off_mode();
  test_runtime_mode_change();
  test_config_change_gate_channel();
  test_config_change_gate_note();
  test_config_change_dac_mode_velocity_to_pitch();
  test_config_change_dac_mode_to_cc();
  test_config_change_cc_num();
  test_config_change_dac_channel();
  test_config_sequence_full_workflow();
  test_gate_held_with_overlapping_notes();
  test_gate_held_release_in_any_order();
  test_gate_specific_note_overlapping();
  test_cross_channel_note_independence();
  test_cc_dac_independent_of_gate();
  test_velocity_cross_channel_fallback();
  test_gate_channel_change_clears_gate();
  test_gate_note_change_clears_gate();
  test_dac_channel_change_clears_stack();
  test_dac_mode_change_clears_value();
  test_deserialize_clears_runtime();
  printf("\nAll tests passed!\n");
  return 0;
}
