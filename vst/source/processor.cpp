#include "processor.h"
#include "cids.h"
#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace tram8 {

// 1V/oct pitch lookup: MIDI notes 0-60 (C-2 to C3), 12-bit DAC, 5V range
// Values are left-justified in 16-bit (actual DAC value = entry >> 4)
const uint16_t Processor::pitchLookup[61] = {
    0x0000, 0x0440, 0x0880, 0x0CD0, 0x1110, 0x1550, 0x19A0, 0x1DE0, 0x2220, 0x2660, 0x2AA0, 0x2EF0, 0x3330,
    0x3770, 0x3BC0, 0x4000, 0x4440, 0x4880, 0x4CC0, 0x5110, 0x5550, 0x5990, 0x5DE0, 0x6220, 0x6660, 0x6AA0,
    0x6EE0, 0x7330, 0x7770, 0x7BB0, 0x8000, 0x8440, 0x8880, 0x8CC0, 0x9100, 0x9550, 0x9990, 0x9DD0, 0xA220,
    0xA660, 0xAAA0, 0xAEE0, 0xB320, 0xB770, 0xBBB0, 0xBFF0, 0xC440, 0xC880, 0xCCC0, 0xD100, 0xD550, 0xD990,
    0xDDD0, 0xE210, 0xE660, 0xEAA0, 0xEEE0, 0xF320, 0xF760, 0xFBB0, 0xFFF0,
};

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

  for (int i = 0; i < TRAM8_NUM_GATES; i++) {
    filters[i].channel = -1;
    filters[i].note = 60 + i;
    dacChannel[i] = -1;
    ccNum[i] = 1;
  }

  openMidiOutput();
  return kResultOk;
}

tresult PLUGIN_API Processor::terminate() {
  closeMidiOutput();
  return AudioEffect::terminate();
}

tresult PLUGIN_API Processor::setActive(TBool state) {
  if (!state) {
    gateMask = 0;
    memset(dacValues, 0, sizeof(dacValues));
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
  // Read parameter changes
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
      if (id >= kGateChannelBase && id < kGateChannelBase + TRAM8_NUM_GATES) {
        int gate = id - kGateChannelBase;
        int step = (int)(value * 16 + 0.5);
        filters[gate].channel = (step == 0) ? -1 : (int8_t)(step - 1);
        os_log(logger, "gate %d channel -> %d", gate, filters[gate].channel);
      } else if (id >= kGateNoteBase && id < kGateNoteBase + TRAM8_NUM_GATES) {
        int gate = id - kGateNoteBase;
        int step = (int)(value * 128 + 0.5);
        filters[gate].note = (step == 0) ? -1 : (int16_t)(step - 1);
        os_log(logger, "gate %d note -> %d", gate, filters[gate].note);
      } else if (id >= kDacModeBase && id < kDacModeBase + TRAM8_NUM_GATES) {
        int gate = id - kDacModeBase;
        int step = (int)(value * (kDacModeCount - 1) + 0.5);
        dacMode[gate] = (uint8_t)step;
        os_log(logger, "gate %d dac mode -> %d", gate, dacMode[gate]);
      } else if (id >= kDacChannelBase && id < kDacChannelBase + TRAM8_NUM_GATES) {
        int gate = id - kDacChannelBase;
        int step = (int)(value * 16 + 0.5);
        dacChannel[gate] = (step == 0) ? -1 : (int8_t)(step - 1);
        os_log(logger, "gate %d dac channel -> %d", gate, dacChannel[gate]);
      } else if (id >= kCcNumBase && id < kCcNumBase + TRAM8_NUM_GATES) {
        int gate = id - kCcNumBase;
        int step = (int)(value * 127 + 0.5);
        ccNum[gate] = (uint8_t)step;
        os_log(logger, "gate %d cc num -> %d", gate, ccNum[gate]);
      } else if (id >= kCcValueBase && id < kCcValueBase + 128) {
        int cc = id - kCcValueBase;
        uint8_t val = (uint8_t)(value * 127 + 0.5);
        ccValues[cc] = val;
        for (int g = 0; g < TRAM8_NUM_GATES; g++) {
          if (dacMode[g] == kDacCC && ccNum[g] == cc && (gateMask & (1 << g))) {
            dacValues[g] = (uint16_t)(val * (TRAM8_DAC_MAX / 127.0f));
          }
        }
      }
    }
  }

  // Silence output
  if (data.numOutputs > 0) {
    for (int32 ch = 0; ch < data.outputs[0].numChannels; ch++) {
      memset(data.outputs[0].channelBuffers32[ch], 0, sizeof(float) * data.numSamples);
    }
  }

  if (!data.inputEvents)
    return kResultOk;

  int32 eventCount = data.inputEvents->getEventCount();
  for (int32 i = 0; i < eventCount; i++) {
    Event e;
    if (data.inputEvents->getEvent(i, e) != kResultOk)
      continue;

    if (e.type == Event::kNoteOnEvent) {
      for (int g = 0; g < TRAM8_NUM_GATES; g++) {
        bool chMatch = (filters[g].channel == -1) || (filters[g].channel == e.noteOn.channel);
        bool noteMatch = (filters[g].note == -1) || (filters[g].note == e.noteOn.pitch);
        if (chMatch && noteMatch) {
          gateMask |= (1 << g);
          switch (dacMode[g]) {
            case kDacPitch: {
              int note = e.noteOn.pitch;
              if (note < 0)
                note = 0;
              if (note > 60)
                note = 60;
              dacValues[g] = pitchLookup[note] >> 4;
              break;
            }
            case kDacCC:
              dacValues[g] = (uint16_t)(ccValues[ccNum[g]] * (TRAM8_DAC_MAX / 127.0f));
              break;
            case kDacOff:
              dacValues[g] = 0;
              break;
            default:
              dacValues[g] = (uint16_t)(e.noteOn.velocity * (float)TRAM8_DAC_MAX);
              break;
          }
          os_log(logger, "gate %d ON  dac=%d mode=%d", g, dacValues[g], dacMode[g]);
        }
      }
    } else if (e.type == Event::kNoteOffEvent) {
      for (int g = 0; g < TRAM8_NUM_GATES; g++) {
        bool chMatch = (filters[g].channel == -1) || (filters[g].channel == e.noteOff.channel);
        bool noteMatch = (filters[g].note == -1) || (filters[g].note == e.noteOff.pitch);
        if (chMatch && noteMatch) {
          gateMask &= ~(1 << g);
          dacValues[g] = 0;
          os_log(logger, "gate %d OFF", g);
        }
      }
    }
  }

  if (stateChanged()) {
    sendState();
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
  for (int i = 0; i < TRAM8_NUM_GATES; i++) {
    int32 ch = filters[i].channel;
    int32 note = filters[i].note;
    int32 mode = dacMode[i];
    int32 dCh = dacChannel[i];
    int32 ccN = ccNum[i];
    state->write(&ch, sizeof(int32));
    state->write(&note, sizeof(int32));
    state->write(&mode, sizeof(int32));
    state->write(&dCh, sizeof(int32));
    state->write(&ccN, sizeof(int32));
  }
  return kResultOk;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
  if (!state)
    return kResultFalse;
  for (int i = 0; i < TRAM8_NUM_GATES; i++) {
    int32 ch = 0, note = 0, mode = 0, dCh = -1, ccN = 1;
    if (state->read(&ch, sizeof(int32)) != kResultOk)
      break;
    if (state->read(&note, sizeof(int32)) != kResultOk)
      break;
    if (state->read(&mode, sizeof(int32)) != kResultOk)
      break;
    if (state->read(&dCh, sizeof(int32)) != kResultOk)
      dCh = -1;
    if (state->read(&ccN, sizeof(int32)) != kResultOk)
      ccN = 1;
    filters[i].channel = (int8_t)ch;
    filters[i].note = (int16_t)note;
    dacMode[i] = (uint8_t)mode;
    dacChannel[i] = (int8_t)dCh;
    ccNum[i] = (uint8_t)ccN;
  }
  return kResultOk;
}

bool Processor::stateChanged() const {
  if (gateMask != prevGateMask)
    return true;
  return memcmp(dacValues, prevDacValues, sizeof(dacValues)) != 0;
}

void Processor::sendState() {
  uint8_t buf[TRAM8_STATE_MSG_LEN];
  tram8_pack_state(buf, gateMask, dacValues);
  sendBytes(buf, TRAM8_STATE_MSG_LEN);

  prevGateMask = gateMask;
  memcpy(prevDacValues, dacValues, sizeof(dacValues));
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

void Processor::sendBytes(const uint8_t* data, uint32_t length) {
  if (!midiOutPort || !midiDest)
    return;

  uint8_t buf[512];
  MIDIPacketList* packetList = (MIDIPacketList*)buf;
  MIDIPacket* packet = MIDIPacketListInit(packetList);
  packet = MIDIPacketListAdd(packetList, sizeof(buf), packet, 0, length, data);
  if (packet) {
    MIDISend(midiOutPort, midiDest, packetList);
  }
}

#else
void Processor::openMidiOutput() {}
void Processor::closeMidiOutput() {}
void Processor::sendBytes(const uint8_t*, uint32_t) {}
#endif

} // namespace tram8
