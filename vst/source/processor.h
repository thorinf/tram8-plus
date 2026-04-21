#pragma once

#include "../../protocol/tram8_sysex.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <atomic>

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <os/log.h>
#endif

namespace tram8 {

struct GateFilter {
  int8_t channel = -1; // -1 = any
  int16_t note = -1; // -1 = any
};

struct NoteEntry {
  int16_t note = 0;
  uint8_t velocity = 0;
};

struct NoteStack {
  static constexpr int kMaxNotes = 16;
  NoteEntry entries[kMaxNotes];
  int count = 0;

  void push(int16_t note, uint8_t velocity) {
    // Remove if already present (re-trigger)
    remove(note);
    if (count < kMaxNotes) {
      entries[count].note = note;
      entries[count].velocity = velocity;
      count++;
    }
  }

  void remove(int16_t note) {
    for (int i = 0; i < count; i++) {
      if (entries[i].note == note) {
        for (int j = i; j < count - 1; j++)
          entries[j] = entries[j + 1];
        count--;
        return;
      }
    }
  }

  bool empty() const { return count == 0; }

  const NoteEntry& top() const { return entries[count - 1]; }
};

class Processor : public Steinberg::Vst::AudioEffect {
 public:
  Processor();
  ~Processor() override;

  static Steinberg::FUnknown* createInstance(void*) {
    return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor());
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
  Steinberg::tresult PLUGIN_API terminate() override;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
  Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement* inputs,
                                                   Steinberg::int32 numIns,
                                                   Steinberg::Vst::SpeakerArrangement* outputs,
                                                   Steinberg::int32 numOuts) override;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) override;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;

 private:
  GateFilter filters[TRAM8_NUM_GATES];
  NoteStack noteStacks[TRAM8_NUM_GATES];
  uint8_t dacMode[TRAM8_NUM_GATES] = {};
  int8_t dacChannel[TRAM8_NUM_GATES] = {};
  uint8_t ccNum[TRAM8_NUM_GATES] = {};
  uint8_t ccValues[128] = {};
  uint8_t gateMask = 0;
  uint16_t dacValues[TRAM8_NUM_GATES] = {};
  uint8_t prevGateMask = 0;
  uint16_t prevDacValues[TRAM8_NUM_GATES] = {};

  static const uint16_t pitchLookup[61];

  void updateDac(int gate, int16_t note, uint8_t velocity);
  bool stateChanged() const;
  void sendState();

#ifdef __APPLE__
  MIDIClientRef midiClient = 0;
  MIDIPortRef midiOutPort = 0;
  std::atomic<MIDIEndpointRef> midiDest{0};
  os_log_t logger = nullptr;
#endif

  void openMidiOutput();
  void closeMidiOutput();
  bool sendBytes(const uint8_t* data, uint32_t length);
};

} // namespace tram8
