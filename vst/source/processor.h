#pragma once

#include "../../protocol/tram8_sysex.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <os/log.h>
#endif

namespace tram8 {

struct GateFilter {
  int8_t channel = -1; // -1 = any
  int16_t note = -1;   // -1 = any
};

class Processor : public Steinberg::Vst::AudioEffect {
public:
  Processor();
  ~Processor() override;

  static Steinberg::FUnknown *createInstance(void *) {
    return static_cast<Steinberg::Vst::IAudioProcessor *>(new Processor());
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) override;
  Steinberg::tresult PLUGIN_API terminate() override;
  Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
  Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData &data) override;
  Steinberg::tresult PLUGIN_API setBusArrangements(Steinberg::Vst::SpeakerArrangement *inputs, Steinberg::int32 numIns,
                                                   Steinberg::Vst::SpeakerArrangement *outputs,
                                                   Steinberg::int32 numOuts) override;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage *message) override;
  Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state) override;
  Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state) override;

private:
  GateFilter filters[TRAM8_NUM_GATES];
  uint8_t dacMode[TRAM8_NUM_GATES] = {};
  uint8_t gateMask = 0;
  uint16_t dacValues[TRAM8_NUM_GATES] = {};
  uint8_t prevGateMask = 0;
  uint16_t prevDacValues[TRAM8_NUM_GATES] = {};

  static const uint16_t pitchLookup[61];

  bool stateChanged() const;
  void sendState();

#ifdef __APPLE__
  MIDIClientRef midiClient = 0;
  MIDIPortRef midiOutPort = 0;
  MIDIEndpointRef midiDest = 0;
  os_log_t logger = nullptr;

  void openMidiOutput();
  void closeMidiOutput();
  void sendBytes(const uint8_t *data, uint32_t length);
#endif
};

} // namespace tram8
