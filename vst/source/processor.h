#pragma once

#include "midi_engine.h"
#include "public.sdk/source/vst/vstaudioeffect.h"

#include <atomic>

#ifdef __APPLE__
#include <CoreMIDI/CoreMIDI.h>
#include <os/log.h>
#endif

namespace tram8 {

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
  MidiEngine engine_;

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
