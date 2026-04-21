#include "processor.h"
#include "cids.h"
#include "../../protocol/tram8_sysex.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace tram8 {

Processor::Processor() {
  setControllerClass(kControllerUID);
}

Processor::~Processor() = default;

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
  tresult result = AudioEffect::initialize(context);
  if (result != kResultOk)
    return result;

  logger = os_log_create("com.thorinf.tram8bridge", "midi");
  os_log(logger, "tram8+ initialized");

  addEventInput(STR16("MIDI In"), 1);
  addAudioOutput(STR16("Audio Out"), SpeakerArr::kStereo);

  engine_.reset();
  openMidiOutput();
  return kResultOk;
}

tresult PLUGIN_API Processor::terminate() {
  closeMidiOutput();
  return AudioEffect::terminate();
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  if (!state) {
    engine_.clearRuntime();
    sendState();
  }
  return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::setBusArrangements(SpeakerArrangement* inputs,
                                                 int32 numIns,
                                                 SpeakerArrangement* outputs,
                                                 int32 numOuts) {
  if (numIns == 0 && numOuts == 1 && outputs[0] == SpeakerArr::kStereo) {
    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
  }
  return kResultFalse;
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
  if (data.inputParameterChanges) {
    int32 numChanged = data.inputParameterChanges->getParameterCount();
    for (int32 idx = 0; idx < numChanged; idx++) {
      auto* queue = data.inputParameterChanges->getParameterData(idx);
      if (!queue)
        continue;

      ParamValue value;
      int32 sampleOffset;
      int32 numPoints = queue->getPointCount();
      if (numPoints <= 0)
        continue;
      if (queue->getPoint(numPoints - 1, sampleOffset, value) != kResultTrue)
        continue;

      ParamID id = queue->getParameterId();
      if (id >= kGateChannelBase && id < kGateChannelBase + kNumGates) {
        int gate = id - kGateChannelBase;
        int step = (int)(value * 16 + 0.5);
        engine_.setGateChannel(gate, (step == 0) ? -1 : (int8_t)(step - 1));
      } else if (id >= kGateNoteBase && id < kGateNoteBase + kNumGates) {
        int gate = id - kGateNoteBase;
        int step = (int)(value * 128 + 0.5);
        engine_.setGateNote(gate, (step == 0) ? -1 : (int16_t)(step - 1));
      } else if (id >= kDacModeBase && id < kDacModeBase + kNumGates) {
        int gate = id - kDacModeBase;
        int step = (int)(value * (kDacModeCount - 1) + 0.5);
        engine_.setDacMode(gate, (uint8_t)step);
      } else if (id >= kDacChannelBase && id < kDacChannelBase + kNumGates) {
        int gate = id - kDacChannelBase;
        int step = (int)(value * 16 + 0.5);
        engine_.setDacChannel(gate, (step == 0) ? -1 : (int8_t)(step - 1));
      } else if (id >= kCcNumBase && id < kCcNumBase + kNumGates) {
        int gate = id - kCcNumBase;
        int step = (int)(value * 127 + 0.5);
        engine_.setCcNum(gate, (uint8_t)step);
      } else if (id >= kCcValueBase && id < kCcValueBase + 128) {
        int cc = id - kCcValueBase;
        engine_.setCcValue((uint8_t)cc, (uint8_t)(value * 127 + 0.5));
      }
    }
  }

  if (data.numOutputs > 0) {
    for (int32 ch = 0; ch < data.outputs[0].numChannels; ch++) {
      memset(data.outputs[0].channelBuffers32[ch], 0, sizeof(float) * data.numSamples);
    }
  }

  bool hadInput = false;
  int32 eventCount = data.inputEvents ? data.inputEvents->getEventCount() : 0;
  hadInput = eventCount > 0;

  for (int32 i = 0; i < eventCount; i++) {
    Event e;
    if (data.inputEvents->getEvent(i, e) != kResultOk)
      continue;

    if (e.type == Event::kNoteOnEvent) {
      os_log(logger, "note on: ch=%d note=%d vel=%.3f", e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity);
      engine_.noteOn(e.noteOn.channel, e.noteOn.pitch, e.noteOn.velocity);
    } else if (e.type == Event::kNoteOffEvent) {
      os_log(logger, "note off: ch=%d note=%d", e.noteOff.channel, e.noteOff.pitch);
      engine_.noteOff(e.noteOff.channel, e.noteOff.pitch);
    }
  }

  bool hadOutput = engine_.stateChanged();
  if (hadOutput)
    sendState();

  if (hadInput || hadOutput) {
    if (auto* msg = allocateMessage()) {
      msg->setMessageID("MidiActivity");
      if (hadInput)
        msg->getAttributes()->setInt("input", 1);
      if (hadOutput)
        msg->getAttributes()->setInt("output", 1);
      sendMessage(msg);
      msg->release();
    }
  }

  return kResultOk;
}

tresult PLUGIN_API Processor::notify(IMessage* message) {
  if (!message)
    return kInvalidArgument;

  if (strcmp(message->getMessageID(), "SetMIDIPort") == 0) {
    int64 index = -1;
    if (message->getAttributes()->getInt("index", index) == kResultOk) {
      if (index < 0) {
        midiDest = 0;
        os_log(logger, "MIDI output: none");
      } else {
        ItemCount destCount = MIDIGetNumberOfDestinations();
        if ((ItemCount)index < destCount) {
          midiDest = MIDIGetDestination((ItemCount)index);
          os_log(logger, "MIDI output: port %lld", index);
        }
      }
    }
    return kResultOk;
  }

  return AudioEffect::notify(message);
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
  if (!state)
    return kResultFalse;
  int32_t buf[kNumGates * MidiEngine::kStateWordsPerGate];
  engine_.serialize(buf);
  return state->write(buf, sizeof(buf)) == kResultOk ? kResultOk : kResultFalse;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
  if (!state)
    return kResultFalse;
  int32_t buf[kNumGates * MidiEngine::kStateWordsPerGate];
  engine_.serialize(buf);

  for (int i = 0; i < kNumGates; i++) {
    int32_t ch, note, mode;
    int32_t dCh = -1, ccN = 1;
    if (state->read(&ch, sizeof(ch)) != kResultOk)
      break;
    if (state->read(&note, sizeof(note)) != kResultOk)
      break;
    if (state->read(&mode, sizeof(mode)) != kResultOk)
      break;
    if (state->read(&dCh, sizeof(dCh)) != kResultOk) {
      dCh = -1;
      ccN = 1;
    } else if (state->read(&ccN, sizeof(ccN)) != kResultOk) {
      ccN = 1;
    }

    int off = i * MidiEngine::kStateWordsPerGate;
    buf[off + 0] = ch;
    buf[off + 1] = note;
    buf[off + 2] = mode;
    buf[off + 3] = dCh;
    buf[off + 4] = ccN;
  }

  engine_.deserialize(buf);
  return kResultOk;
}

void Processor::sendState() {
  tram8_form_t form = TRAM8_FORM_GATES;
  if (engine_.dacChanged() && engine_.hasPitchMode()) {
    form = TRAM8_FORM_FULL;
  } else if (engine_.dacChanged()) {
    form = TRAM8_FORM_COARSE;
  }

  uint16_t dac12[kNumGates];
  for (int i = 0; i < kNumGates; i++)
    dac12[i] = engine_.dacValues()[i] >> 2;

  uint8_t buf[TRAM8_LEN_FULL];
  uint8_t len = tram8_pack(buf, engine_.gateMask(), dac12, form);

  static const char* formNames[] = {"gates", "coarse", "full"};
  os_log(logger,
         "send [%{public}s %dB] gates=0x%02X dac=[%u %u %u %u %u %u %u %u]",
         formNames[form],
         len,
         engine_.gateMask(),
         dac12[0],
         dac12[1],
         dac12[2],
         dac12[3],
         dac12[4],
         dac12[5],
         dac12[6],
         dac12[7]);

  if (!sendBytes(buf, len))
    return;

  engine_.markSent();
}

#ifdef __APPLE__

void Processor::openMidiOutput() {
  OSStatus status = MIDIClientCreate(CFSTR("tram8+"), nullptr, nullptr, &midiClient);
  if (status != noErr) {
    os_log_error(logger, "failed to create MIDI client (%d)", (int)status);
    return;
  }

  status = MIDIOutputPortCreate(midiClient, CFSTR("tram8+ out"), &midiOutPort);
  if (status != noErr) {
    os_log_error(logger, "failed to create MIDI output port (%d)", (int)status);
    MIDIClientDispose(midiClient);
    midiClient = 0;
    return;
  }

  ItemCount destCount = MIDIGetNumberOfDestinations();
  for (ItemCount i = 0; i < destCount; i++) {
    MIDIEndpointRef ep = MIDIGetDestination(i);
    CFStringRef name = nullptr;
    MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &name);
    if (name) {
      char buf[256];
      CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8);
      os_log(logger, "found MIDI destination [%lu]: %{public}s", i, buf);
      CFRelease(name);
    }
    if (midiDest == 0) {
      midiDest = ep;
    }
  }

  if (midiDest) {
    os_log(logger, "MIDI output ready");
  } else {
    os_log_error(logger, "no MIDI destinations found");
  }
}

void Processor::closeMidiOutput() {
  if (midiOutPort)
    MIDIPortDispose(midiOutPort);
  if (midiClient)
    MIDIClientDispose(midiClient);
  midiOutPort = 0;
  midiClient = 0;
  midiDest = 0;
}

bool Processor::sendBytes(const uint8_t* data, uint32_t length) {
  if (!midiOutPort || !midiDest)
    return false;

  uint8_t buf[512];
  MIDIPacketList* packetList = (MIDIPacketList*)buf;
  MIDIPacket* packet = MIDIPacketListInit(packetList);
  packet = MIDIPacketListAdd(packetList, sizeof(buf), packet, 0, length, data);
  if (!packet)
    return false;

  return MIDISend(midiOutPort, midiDest, packetList) == noErr;
}

#else
void Processor::openMidiOutput() {}
void Processor::closeMidiOutput() {}
bool Processor::sendBytes(const uint8_t*, uint32_t) {
  return false;
}
#endif

} // namespace tram8
