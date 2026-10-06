#include "../source/midi_engine.h"
#include "../source/state_format.h"
#include "../../protocol/tram8_sysex.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include <type_traits>

using namespace tram8;

static_assert(
    std::is_constructible_v<std::span<int32_t, MidiEngine::kStateWordCount>, int32_t (&)[MidiEngine::kStateWordCount]>);
static_assert(!std::is_constructible_v<std::span<int32_t, MidiEngine::kStateWordCount>,
                                       int32_t (&)[MidiEngine::kStateWordCount - 1]>);

class TestStream : public Steinberg::IBStream {
 public:
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID, void** obj) override {
    if (obj)
      *obj = nullptr;
    return Steinberg::kNoInterface;
  }

  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
  Steinberg::uint32 PLUGIN_API release() override { return 1; }

  Steinberg::tresult PLUGIN_API read(void* buffer, Steinberg::int32 numBytes, Steinberg::int32* numBytesRead) override {
    if (!buffer || numBytes < 0)
      return Steinberg::kInvalidArgument;
    size_t available = data_.size() - cursor_;
    size_t count = std::min<size_t>((size_t)numBytes, available);
    if (count > 0)
      memcpy(buffer, data_.data() + cursor_, count);
    cursor_ += count;
    if (numBytesRead)
      *numBytesRead = (Steinberg::int32)count;
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API write(void* buffer,
                                      Steinberg::int32 numBytes,
                                      Steinberg::int32* numBytesWritten) override {
    if (!buffer || numBytes < 0)
      return Steinberg::kInvalidArgument;
    size_t required = cursor_ + (size_t)numBytes;
    if (data_.size() < required)
      data_.resize(required);
    memcpy(data_.data() + cursor_, buffer, (size_t)numBytes);
    cursor_ = required;
    if (numBytesWritten)
      *numBytesWritten = numBytes;
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API seek(Steinberg::int64 pos, Steinberg::int32 mode, Steinberg::int64* result) override {
    Steinberg::int64 next = (Steinberg::int64)cursor_;
    if (mode == Steinberg::IBStream::kIBSeekSet)
      next = pos;
    else if (mode == Steinberg::IBStream::kIBSeekCur)
      next += pos;
    else if (mode == Steinberg::IBStream::kIBSeekEnd)
      next = (Steinberg::int64)data_.size() + pos;
    else
      return Steinberg::kInvalidArgument;
    if (next < 0 || next > (Steinberg::int64)data_.size())
      return Steinberg::kInvalidArgument;
    cursor_ = (size_t)next;
    if (result)
      *result = next;
    return Steinberg::kResultOk;
  }

  Steinberg::tresult PLUGIN_API tell(Steinberg::int64* pos) override {
    if (!pos)
      return Steinberg::kInvalidArgument;
    *pos = (Steinberg::int64)cursor_;
    return Steinberg::kResultOk;
  }

  void rewind() { cursor_ = 0; }

 private:
  std::vector<uint8_t> data_;
  size_t cursor_ = 0;
};

static void write_test_int(TestStream& stream, int32_t value) {
  assert(streamWriteInt32(&stream, value));
}

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

static void test_note_stack_remove_middle_and_missing() {
  NoteStack stack;
  stack.remove(0, 60);
  stack.push(0, 60, 100);
  stack.push(1, 60, 80);
  stack.push(0, 64, 90);
  stack.remove(1, 60);
  stack.remove(2, 60);
  assert(stack.count == 2);
  assert(stack.entries[0].note == 60 && stack.entries[0].velocity == 100);
  assert(stack.top().note == 64 && stack.top().velocity == 90);
  stack.remove(0, 64);
  assert(stack.top().note == 60);
  printf("note_stack_remove_middle_and_missing passed\n");
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
  uint16_t half = (uint16_t)(0.5f * 127.0f + 0.5f) << 7;
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

static void test_pitch_table_and_bounds() {
  static_assert(MidiEngine::pitchLookup.front() == 0x0000);
  static_assert(MidiEngine::pitchLookup[30] == 0x8000);
  static_assert(MidiEngine::pitchLookup.back() == 0xFFF0);
  MidiEngine engine;
  engine.setDacMode(0, kDacPitch);
  for (int note = -1; note <= 61; note++) {
    engine.noteOn(0, (int16_t)note, 1.f);
    int index = std::ranges::clamp(note, 0, 60);
    assert(engine.dacValues()[0] == ((MidiEngine::pitchLookup[index] >> 2) & 0x3FFC));
    engine.noteOff(0, (int16_t)note);
  }
  printf("pitch_table_and_bounds passed\n");
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

  engine.setCcValue(0, 1, 100);
  assert(engine.dacValues()[0] == (uint16_t)100 << 7);

  engine.setCcValue(0, 1, 0);
  assert(engine.dacValues()[0] == 0);

  printf("cc_mode passed\n");
}

static void test_cc_mixed_channels_and_gate_independence() {
  MidiEngine engine;
  for (int g = 0; g < 3; g++) {
    engine.setGateChannel(g, 4);
    engine.setGateNote(g, 60);
    engine.setDacMode(g, kDacCC);
    engine.setCcNum(g, 7);
  }
  engine.setDacChannel(0, 0);
  engine.setDacChannel(1, kMidiChannelCount - 1);
  engine.noteOn(4, 60, 1.f);
  assert(engine.gateMask() == 0x07);

  for (int channel = 0; channel < kMidiChannelCount; channel++)
    engine.setCcValue(channel, 7, (uint8_t)(10 + channel));
  assert(engine.dacValues()[0] == 10 << 7);
  assert(engine.dacValues()[1] == 25 << 7);
  assert(engine.dacValues()[2] == 25 << 7);
  assert(engine.gateMask() == 0x07);

  engine.setCcValue(0, 7, 0);
  assert(engine.dacValues()[0] == 0);
  assert(engine.dacValues()[1] == 25 << 7);
  assert(engine.dacValues()[2] == 0);

  engine.setCcValue(1, 8, 127);
  engine.noteOn(15, 64, 1.f);
  engine.noteOff(15, 64);
  assert(engine.dacValues()[0] == 0);
  assert(engine.dacValues()[1] == 25 << 7);
  assert(engine.dacValues()[2] == 0);
  assert(engine.gateMask() == 0x07);
  engine.noteOff(4, 60);
  assert(engine.gateMask() == 0);

  printf("cc_mixed_channels_and_gate_independence passed\n");
}

static void test_cc_config_repopulates_matching_cache() {
  MidiEngine engine;
  engine.setCcValue(0, 7, 100);
  engine.setCcValue(1, 7, 50);
  engine.setCcValue(0, 11, 30);
  engine.setCcValue(1, 11, 80);
  engine.setCcNum(0, 7);
  engine.setDacChannel(0, 0);
  engine.setDacMode(0, kDacCC);
  assert(engine.dacValues()[0] == 100 << 7);

  engine.setDacChannel(0, 1);
  assert(engine.dacValues()[0] == 50 << 7);
  engine.setDacChannel(0, 2);
  assert(engine.dacValues()[0] == 0);
  engine.setDacChannel(0, -1);
  assert(engine.dacValues()[0] == 50 << 7);

  engine.setCcNum(0, 11);
  assert(engine.dacValues()[0] == 80 << 7);
  engine.setDacChannel(0, 0);
  assert(engine.dacValues()[0] == 30 << 7);
  engine.setCcNum(0, 127);
  assert(engine.dacValues()[0] == 0);
  engine.setCcValue(0, 127, 127);
  assert(engine.dacValues()[0] == 127 << 7);

  engine.setDacMode(0, kDacOff);
  assert(engine.dacValues()[0] == 0);
  engine.setCcValue(0, 127, 60);
  engine.setCcValue(1, 127, 90);
  engine.setDacMode(0, kDacCC);
  assert(engine.dacValues()[0] == 60 << 7);
  engine.setDacChannel(0, -1);
  assert(engine.dacValues()[0] == 90 << 7);

  printf("cc_config_repopulates_matching_cache passed\n");
}

static void test_cc_any_uses_latest_sample_offset() {
  MidiEngine engine;
  engine.setDacMode(0, kDacCC);
  engine.setCcNum(0, 7);
  engine.setDacMode(1, kDacCC);
  engine.setCcNum(1, 7);
  engine.setDacChannel(1, 1);

  engine.beginCcBlock();
  engine.setCcValue(15, 7, 100, 10);
  engine.setCcValue(0, 7, 40, 50);
  engine.setCcValue(1, 7, 80, 20);
  assert(engine.dacValues()[0] == 40 << 7);
  assert(engine.dacValues()[1] == 80 << 7);
  engine.setDacChannel(0, 15);
  assert(engine.dacValues()[0] == 100 << 7);
  engine.setDacChannel(0, -1);
  assert(engine.dacValues()[0] == 40 << 7);

  engine.beginCcBlock();
  engine.setCcValue(15, 7, 0, 0);
  assert(engine.dacValues()[0] == 0);
  assert(engine.dacValues()[1] == 80 << 7);

  printf("cc_any_uses_latest_sample_offset passed\n");
}

static void test_cc_runtime_clear_discards_cache() {
  MidiEngine engine;
  engine.setDacMode(0, kDacCC);
  engine.setCcNum(0, 7);
  engine.setCcValue(0, 7, 100);
  engine.setCcValue(1, 7, 50);
  int32_t words[kNumGates * MidiEngine::kStateWordsPerGate];
  engine.serialize(words);

  engine.clearRuntime();
  assert(engine.dacValues()[0] == 0);
  engine.setDacChannel(0, 0);
  assert(engine.dacValues()[0] == 0);
  engine.setDacChannel(0, -1);
  assert(engine.dacValues()[0] == 0);

  engine.setCcValue(1, 7, 80);
  engine.deserialize(words);
  engine.setDacChannel(0, 1);
  assert(engine.dacValues()[0] == 0);
  engine.setDacChannel(0, -1);
  assert(engine.dacValues()[0] == 0);

  engine.setCcValue(1, 7, 90);
  engine.reset();
  engine.setCcNum(0, 7);
  engine.setDacMode(0, kDacCC);
  assert(engine.dacValues()[0] == 0);
  engine.setDacChannel(0, 1);
  assert(engine.dacValues()[0] == 0);

  printf("cc_runtime_clear_discards_cache passed\n");
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

static void test_velocity_follows_gate_filter() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 61, 1.0f);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  engine.noteOff(0, 61);
  assert(engine.dacValues()[0] == 0);

  engine.noteOn(0, 60, 0.5f);
  assert(engine.gateMask() & 1);
  uint16_t expected = (uint16_t)(0.5f * 127.0f + 0.5f) << 7;
  assert(engine.dacValues()[0] == expected);

  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  printf("velocity_follows_gate_filter passed\n");
}

static void test_velocity_unaffected_by_non_gate_notes() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  uint16_t cVel = (uint16_t)(0.8f * 127.0f + 0.5f) << 7;
  assert(engine.dacValues()[0] == cVel);

  engine.noteOn(0, 61, 0.2f);
  assert(engine.dacValues()[0] == cVel);

  engine.noteOff(0, 61);
  assert(engine.dacValues()[0] == cVel);

  engine.noteOff(0, 60);
  assert(engine.dacValues()[0] == 0);

  printf("velocity_unaffected_by_non_gate_notes passed\n");
}

static void test_velocity_rollback_with_overlap() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.4f);
  uint16_t v60 = (uint16_t)(0.4f * 127.0f + 0.5f) << 7;
  assert(engine.dacValues()[0] == v60);

  engine.noteOn(0, 62, 0.9f);
  uint16_t v62 = (uint16_t)(0.9f * 127.0f + 0.5f) << 7;
  assert(engine.dacValues()[0] == v62);

  engine.noteOff(0, 62);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] == v60);

  engine.noteOff(0, 60);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  printf("velocity_rollback_with_overlap passed\n");
}

static void test_velocity_cleared_on_gate_config_change() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 0.8f);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] > 0);

  engine.setGateNote(0, 72);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  engine.noteOn(0, 72, 0.8f);
  assert(engine.gateMask() & 1);
  assert(engine.dacValues()[0] > 0);

  engine.setGateChannel(0, 5);
  assert(!(engine.gateMask() & 1));
  assert(engine.dacValues()[0] == 0);

  printf("velocity_cleared_on_gate_config_change passed\n");
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

static void test_single_change_detection() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setGateChannel(1, -1);
  engine.setGateNote(1, 61);
  engine.markSent();

  engine.noteOn(0, 60, 0.8f);
  assert(engine.changedGateIndex() == 0);
  assert(engine.changedDacIndex() == 0);

  engine.markSent();
  engine.noteOn(0, 61, 0.6f);
  assert(engine.changedGateIndex() == 1);
  assert(engine.changedDacIndex() == 1);

  printf("single_change_detection passed\n");
}

static void test_single_gate_detection_all_bits() {
  MidiEngine engine;
  engine.markSent();
  assert(engine.changedGateIndex() == -1);
  for (int gate = 0; gate < kNumGates; gate++) {
    engine.beginBlock();
    engine.noteOn(0, 60 + gate, 1.f);
    assert(engine.changedGateIndex() == gate);
    engine.markSent();
    engine.beginBlock();
    engine.noteOff(0, 60 + gate);
    assert(engine.changedGateIndex() == gate);
    engine.markSent();
    assert(engine.changedGateIndex() == -1);
  }
  printf("single_gate_detection_all_bits passed\n");
}

static void test_multi_change_detection() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, 60);
  engine.setGateChannel(1, -1);
  engine.setGateNote(1, 61);
  engine.markSent();

  engine.noteOn(0, 60, 0.8f);
  engine.noteOn(0, 61, 0.6f);
  assert(engine.changedGateIndex() == -1);
  assert(engine.changedDacIndex() == -1);

  printf("multi_change_detection passed\n");
}

static void assert_output_frame(const MidiEngine& engine,
                                uint8_t expectedGates,
                                const uint16_t expectedDac[kNumGates],
                                tram8_form_t form) {
  uint16_t dac12[kNumGates];
  for (int g = 0; g < kNumGates; g++)
    dac12[g] = engine.outputDacValue(g) >> 2;
  uint8_t buf[TRAM8_LEN_FULL];
  uint8_t len = tram8_pack(buf, engine.outputGateMask(), dac12, form);
  uint8_t gates = 0;
  uint16_t decoded[kNumGates] = {};
  tram8_form_t decodedForm;
  assert(tram8_parse(buf, len, &gates, decoded, &decodedForm) == 0);
  assert(decodedForm == form);
  assert(gates == expectedGates);
  assert(memcmp(decoded, expectedDac, sizeof(decoded)) == 0);
}

static void test_same_block_pulse_and_empty_block_release() {
  for (int g = 0; g < kNumGates; g++) {
    MidiEngine engine;
    engine.beginBlock();
    float velocity = (g + 1) / 8.f;
    uint16_t vel = (uint16_t)(velocity * 127.f + 0.5f);
    engine.noteOn(0, 60 + g, velocity);
    engine.noteOff(0, 60 + g);
    assert(engine.gateMask() == 0);
    assert(engine.dacValues()[g] == 0);
    assert(engine.stateChanged());
    assert(engine.changedGateIndex() == g);
    assert(engine.changedDacIndex() == g);
    uint16_t expected[kNumGates] = {};
    expected[g] = vel << 5;
    assert_output_frame(engine, 1 << g, expected, TRAM8_FORM_COARSE);
    assert_output_frame(engine, 1 << g, expected, TRAM8_FORM_FULL);

    engine.markSent();
    assert(!engine.stateChanged());
    assert(engine.gateMask() == 0);
    engine.beginBlock();
    assert(engine.stateChanged());
    assert(engine.changedGateIndex() == g);
    assert(engine.changedDacIndex() == g);
    expected[g] = 0;
    assert_output_frame(engine, 0, expected, TRAM8_FORM_COARSE);
    engine.markSent();
    engine.beginBlock();
    assert(!engine.stateChanged());
  }
  printf("same_block_pulse_and_empty_block_release passed\n");
}

static void test_normal_block_notes_and_releases() {
  MidiEngine engine;
  engine.setGateNote(0, -1);
  engine.beginBlock();
  engine.noteOn(0, 48, 0.25f);
  assert(engine.outputGateMask() == 1);
  assert(engine.outputDacValue(0) == 32 << 7);
  engine.markSent();
  engine.beginBlock();
  assert(!engine.stateChanged());
  assert(engine.gateMask() == 1);
  engine.noteOn(0, 50, 0.75f);
  engine.noteOff(0, 50);
  assert(engine.outputDacValue(0) == 32 << 7);
  engine.noteOff(0, 48);
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 0);
  assert(engine.stateChanged());
  engine.markSent();
  engine.beginBlock();
  assert(!engine.stateChanged());
  printf("normal_block_notes_and_releases passed\n");
}

static void test_pulse_overlap_and_no_phantom_held_note() {
  MidiEngine engine;
  engine.setGateNote(0, -1);
  engine.beginBlock();
  engine.noteOn(0, 48, 0.25f);
  engine.noteOn(0, 50, 0.75f);
  engine.noteOff(0, 50);
  engine.noteOff(0, 48);
  assert(engine.gateMask() == 0);
  assert(engine.outputDacValue(0) == 32 << 7);
  engine.markSent();
  engine.beginBlock();
  engine.noteOn(0, 52, 0.5f);
  engine.noteOff(0, 48);
  assert(engine.gateMask() == 1);
  assert(engine.outputDacValue(0) == 64 << 7);
  engine.markSent();
  engine.beginBlock();
  engine.noteOff(0, 52);
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 0);
  printf("pulse_overlap_and_no_phantom_held_note passed\n");
}

static void test_pulse_cc_independence() {
  MidiEngine engine;
  engine.setDacMode(0, kDacCC);
  engine.setDacChannel(0, 1);
  engine.setCcNum(0, 7);
  engine.setCcValue(1, 7, 32);
  engine.markSent();
  engine.beginBlock();
  engine.noteOn(0, 60, 0.25f);
  engine.noteOn(0, 61, 0.5f);
  engine.setCcValue(1, 7, 95);
  engine.noteOff(0, 60);
  engine.noteOff(0, 61);
  uint16_t expected[kNumGates] = {95 << 5, 64 << 5};
  assert_output_frame(engine, 3, expected, TRAM8_FORM_COARSE);
  engine.markSent();
  engine.beginBlock();
  assert(engine.changedDacIndex() == 1);
  expected[1] = 0;
  assert_output_frame(engine, 0, expected, TRAM8_FORM_COARSE);
  engine.setCcValue(1, 7, 50);
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 50 << 7);
  printf("pulse_cc_independence passed\n");
}

static void test_pulse_pitch_routing_unchanged() {
  for (int channel = 0; channel < 2; channel++) {
    MidiEngine engine;
    engine.setGateNote(0, 48);
    engine.setGateChannel(0, 0);
    engine.setDacMode(0, kDacPitch);
    engine.setDacChannel(0, channel);
    engine.noteOn(channel, 36, 0.25f);
    engine.markSent();
    engine.beginBlock();
    engine.noteOn(0, 48, 0.75f);
    engine.noteOff(0, 48);
    if (channel == 1)
      engine.noteOn(1, 40, 0.5f);
    int pitch = channel == 0 ? 36 : 40;
    uint16_t expected[kNumGates] = {(uint16_t)(MidiEngine::pitchLookup[pitch] >> 4)};
    assert_output_frame(engine, 1, expected, TRAM8_FORM_FULL);
    engine.markSent();
    engine.beginBlock();
    assert(engine.outputGateMask() == 0);
    assert(!engine.dacChanged());
    assert_output_frame(engine, 0, expected, TRAM8_FORM_FULL);
  }
  printf("pulse_pitch_routing_unchanged passed\n");
}

static void test_pulse_pitch_later_independent_note() {
  MidiEngine engine;
  engine.setGateChannel(0, 0);
  engine.setGateNote(0, 48);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);
  engine.beginBlock();
  engine.noteOn(0, 48, 0.75f);
  engine.noteOff(0, 48);
  engine.noteOn(1, 40, 0.5f);
  assert(engine.gateMask() == 0);
  assert(engine.outputDacValue(0) == engine.dacValues()[0]);
  uint16_t expected[kNumGates] = {(uint16_t)(MidiEngine::pitchLookup[40] >> 4)};
  assert_output_frame(engine, 1, expected, TRAM8_FORM_FULL);
  engine.markSent();
  engine.beginBlock();
  assert(engine.stateChanged());
  assert(!engine.dacChanged());
  assert_output_frame(engine, 0, expected, TRAM8_FORM_FULL);
  printf("pulse_pitch_later_independent_note passed\n");
}

static void test_block_zero_velocity_note_off() {
  MidiEngine engine;
  engine.beginBlock();
  engine.noteOn(0, 60, 0.f);
  assert(!engine.stateChanged());
  assert(engine.outputGateMask() == 0);
  engine.noteOn(0, 60, 0.5f);
  engine.noteOn(0, 60, 0.f);
  assert(engine.gateMask() == 0);
  assert(engine.outputGateMask() == 1);
  assert(engine.outputDacValue(0) == 64 << 7);
  engine.markSent();
  engine.beginBlock();
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 0);
  engine.noteOn(0, 60, 0.5f);
  engine.markSent();
  engine.beginBlock();
  engine.noteOn(0, 60, 0.f);
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 0);
  printf("block_zero_velocity_note_off passed\n");
}

static void test_block_pulses_merge_retriggers() {
  MidiEngine engine;
  engine.beginBlock();
  engine.noteOn(0, 60, 0.25f);
  engine.noteOff(0, 60);
  engine.noteOn(0, 60, 0.75f);
  engine.noteOff(0, 60);
  assert(engine.outputGateMask() == 1);
  assert(engine.outputDacValue(0) == 95 << 7);
  engine.markSent();
  engine.beginBlock();
  engine.noteOn(0, 60, 0.75f);
  engine.noteOff(0, 60);
  assert(!engine.stateChanged()); // Adjacent pulses have no transmitted low edge.
  engine.beginBlock();
  assert(engine.stateChanged());
  assert(engine.outputGateMask() == 0);
  printf("block_pulses_merge_retriggers passed\n");
}

static void test_runtime_clear_discards_pulse() {
  MidiEngine engine;
  engine.beginBlock();
  engine.noteOn(0, 60, 0.5f);
  engine.noteOff(0, 60);
  engine.setGateNote(0, 48);
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 0);
  engine.noteOn(0, 48, 0.5f);
  engine.noteOff(0, 48);
  engine.clearRuntime();
  assert(engine.outputGateMask() == 0);
  assert(engine.outputDacValue(0) == 0);
  assert(!engine.stateChanged());
  printf("runtime_clear_discards_pulse passed\n");
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
  engine.setCcValue(0, 7, 64);

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
  engine.setCcValue(0, 1, 100);
  assert(engine.dacValues()[0] == (uint16_t)100 << 7);

  engine.setCcNum(0, 7);
  engine.setCcValue(0, 7, 50);

  assert(engine.dacValues()[0] == (uint16_t)50 << 7);

  engine.setCcValue(0, 1, 127);
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
  engine.setCcValue(0, 11, 80);
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

  engine.setCcValue(1, 7, 64);
  assert(engine.dacValues()[0] == (uint16_t)64 << 7);

  engine.setCcValue(1, 7, 0);
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
  uint16_t ch0Vel = (uint16_t)(0.25f * 127.0f + 0.5f) << 7;
  assert(engine.dacValues()[0] == ch0Vel);

  engine.noteOn(1, 60, 0.75f);
  uint16_t ch1Vel = (uint16_t)(0.75f * 127.0f + 0.5f) << 7;
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

static void test_dac_mode_pitch_to_cc_populates_value() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);
  engine.setCcNum(0, 7);

  engine.setCcValue(0, 7, 100);

  engine.noteOn(0, 48, 0.8f);
  uint16_t pitchVal = engine.dacValues()[0];
  assert(pitchVal > 0);

  engine.setDacMode(0, kDacCC);
  assert(engine.dacValues()[0] == (uint16_t)100 << 7);

  printf("dac_mode_pitch_to_cc_populates_value passed\n");
}

static void test_dac_channel_change_zeros_pitch() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacPitch);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 48, 0.8f);
  assert(engine.dacValues()[0] > 0);

  engine.setDacChannel(0, 0);
  assert(engine.dacValues()[0] == 0);

  printf("dac_channel_change_zeros_pitch passed\n");
}

static void test_note_stack_top_empty() {
  NoteStack stack;
  assert(stack.empty());
  const NoteEntry& e = stack.top();
  assert(e.note == 0);
  assert(e.velocity == 0);
  assert(e.channel == 0);

  printf("note_stack_top_empty passed\n");
}

static void test_cc_updates_without_active_note() {
  MidiEngine engine;
  engine.setDacMode(0, kDacCC);
  engine.setDacChannel(0, -1);
  engine.setCcNum(0, 7);

  engine.setCcValue(0, 7, 64);
  assert(engine.dacValues()[0] == (uint16_t)64 << 7);

  engine.setCcValue(0, 7, 100);
  assert(engine.dacValues()[0] == (uint16_t)100 << 7);

  engine.setCcNum(0, 1);
  engine.setCcValue(0, 1, 50);
  assert(engine.dacValues()[0] == (uint16_t)50 << 7);

  printf("cc_updates_without_active_note passed\n");
}

static void test_dac_mode_to_pitch_zeros_value() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.dacValues()[0] > 0);

  engine.setDacMode(0, kDacPitch);
  assert(engine.dacValues()[0] == 0);

  printf("dac_mode_to_pitch_zeros_value passed\n");
}

static void test_velocity_rounding() {
  MidiEngine engine;
  engine.setGateChannel(0, -1);
  engine.setGateNote(0, -1);
  engine.setDacMode(0, kDacVelocity);
  engine.setDacChannel(0, -1);

  engine.noteOn(0, 60, 1.0f);
  assert(engine.dacValues()[0] == (uint16_t)127 << 7);

  engine.noteOff(0, 60);
  engine.noteOn(0, 60, 0.5f);
  uint16_t halfVel = engine.dacValues()[0];
  assert(halfVel == (uint16_t)64 << 7);

  printf("velocity_rounding passed\n");
}

static void test_out_of_bounds_gate_ignored() {
  MidiEngine engine;
  engine.setGateChannel(-1, 0);
  engine.setGateChannel(8, 0);
  engine.setGateNote(-1, 60);
  engine.setGateNote(8, 60);
  engine.setDacMode(-1, kDacPitch);
  engine.setDacMode(8, kDacPitch);
  engine.setDacChannel(-1, 0);
  engine.setDacChannel(8, 0);
  engine.setCcNum(-1, 7);
  engine.setCcNum(8, 7);
  engine.setDacChannel(0, -2);
  engine.setDacChannel(0, kMidiChannelCount);
  engine.setCcNum(0, kMidiCcCount);
  engine.setCcValue(-1, 1, 64);
  engine.setCcValue(kMidiChannelCount, 1, 64);
  engine.setCcValue(0, kMidiCcCount, 64);
  engine.setCcValue(0, 1, kMidiCcCount);
  engine.setDacMode(0, kDacCC);
  assert(engine.dacValues()[0] == 0);
  engine.clearGateRuntime(-1);
  engine.clearGateRuntime(8);

  assert(engine.gateMask() == 0);
  printf("out_of_bounds_gate_ignored passed\n");
}

static void test_state_format_versioned_roundtrip() {
  std::array<int32_t, MidiEngine::kStateWordCount> words;
  for (int i = 0; i < kNumGates; i++) {
    int off = i * MidiEngine::kStateWordsPerGate;
    words[off + 0] = i - 1;
    words[off + 1] = 60 + i;
    words[off + 2] = i % kDacModeCount;
    words[off + 3] = 15 - i;
    words[off + 4] = 20 + i;
  }

  TestStream stream;
  assert(writeStateWords(&stream, words));
  stream.rewind();

  std::array<int32_t, MidiEngine::kStateWordCount> decoded{};
  assert(readStateWords(&stream, decoded));
  assert(std::ranges::equal(words, decoded));

  printf("state_format_versioned_roundtrip passed\n");
}

static void test_state_format_reads_legacy_five_word_state() {
  int32_t words[kNumGates * MidiEngine::kStateWordsPerGate];
  TestStream stream;
  for (int i = 0; i < kNumGates; i++) {
    int off = i * MidiEngine::kStateWordsPerGate;
    words[off + 0] = i;
    words[off + 1] = 40 + i;
    words[off + 2] = i % kDacModeCount;
    words[off + 3] = 7 - i;
    words[off + 4] = 64 + i;
    for (int field = 0; field < MidiEngine::kStateWordsPerGate; field++)
      write_test_int(stream, words[off + field]);
  }
  stream.rewind();

  int32_t decoded[kNumGates * MidiEngine::kStateWordsPerGate] = {0};
  assert(readStateWords(&stream, decoded));
  assert(memcmp(words, decoded, sizeof(words)) == 0);

  printf("state_format_reads_legacy_five_word_state passed\n");
}

static void test_state_format_reads_legacy_three_word_state() {
  TestStream stream;
  for (int i = 0; i < kNumGates; i++) {
    write_test_int(stream, i);
    write_test_int(stream, 50 + i);
    write_test_int(stream, i % kDacModeCount);
  }
  stream.rewind();

  int32_t decoded[kNumGates * MidiEngine::kStateWordsPerGate] = {0};
  assert(readStateWords(&stream, decoded));
  for (int i = 0; i < kNumGates; i++) {
    int off = i * MidiEngine::kStateWordsPerGate;
    assert(decoded[off + 0] == i);
    assert(decoded[off + 1] == 50 + i);
    assert(decoded[off + 2] == i % kDacModeCount);
    assert(decoded[off + 3] == -1);
    assert(decoded[off + 4] == 1);
  }

  printf("state_format_reads_legacy_three_word_state passed\n");
}

static void test_state_format_rejects_truncated_state() {
  TestStream stream;
  write_test_int(stream, kStateMagic);
  write_test_int(stream, kStateVersion);
  write_test_int(stream, 123);
  stream.rewind();

  int32_t decoded[kNumGates * MidiEngine::kStateWordsPerGate] = {0};
  assert(!readStateWords(&stream, decoded));

  printf("state_format_rejects_truncated_state passed\n");
}

static void test_state_format_rejects_unknown_version() {
  TestStream stream;
  write_test_int(stream, kStateMagic);
  write_test_int(stream, kStateVersion + 1);
  for (int i = 0; i < kNumGates * MidiEngine::kStateWordsPerGate; i++)
    write_test_int(stream, 0);
  stream.rewind();

  int32_t decoded[kNumGates * MidiEngine::kStateWordsPerGate] = {0};
  assert(!readStateWords(&stream, decoded));

  printf("state_format_rejects_unknown_version passed\n");
}

int main() {
  test_note_stack_top_empty();
  test_note_stack_push_pop();
  test_note_stack_retrigger();
  test_note_stack_remove_middle_and_missing();
  test_note_stack_overflow();
  test_note_stack_channel_isolation();
  test_velocity_mode();
  test_velocity_zero_as_note_off();
  test_pitch_mode();
  test_pitch_table_and_bounds();
  test_pitch_hold_on_note_off();
  test_last_note_priority();
  test_cc_mode();
  test_cc_mixed_channels_and_gate_independence();
  test_cc_config_repopulates_matching_cache();
  test_cc_any_uses_latest_sample_offset();
  test_cc_runtime_clear_discards_cache();
  test_gate_note_filter();
  test_gate_channel_filter();
  test_dac_independence();
  test_velocity_follows_gate_filter();
  test_velocity_unaffected_by_non_gate_notes();
  test_velocity_rollback_with_overlap();
  test_velocity_cleared_on_gate_config_change();
  test_state_changed();
  test_dac_changed();
  test_single_change_detection();
  test_single_gate_detection_all_bits();
  test_multi_change_detection();
  test_same_block_pulse_and_empty_block_release();
  test_normal_block_notes_and_releases();
  test_pulse_overlap_and_no_phantom_held_note();
  test_pulse_cc_independence();
  test_pulse_pitch_later_independent_note();
  test_pulse_pitch_routing_unchanged();
  test_block_zero_velocity_note_off();
  test_block_pulses_merge_retriggers();
  test_runtime_clear_discards_pulse();
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
  test_dac_mode_pitch_to_cc_populates_value();
  test_dac_channel_change_zeros_pitch();
  test_cc_updates_without_active_note();
  test_dac_mode_to_pitch_zeros_value();
  test_velocity_rounding();
  test_out_of_bounds_gate_ignored();
  test_state_format_versioned_roundtrip();
  test_state_format_reads_legacy_five_word_state();
  test_state_format_reads_legacy_three_word_state();
  test_state_format_rejects_truncated_state();
  test_state_format_rejects_unknown_version();
  printf("\nAll tests passed!\n");
  return 0;
}
