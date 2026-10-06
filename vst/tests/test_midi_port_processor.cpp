#include "../source/processor.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

static std::vector<MIDIEndpointRef> destinations{101, 202};
static std::vector<MIDIEndpointRef> sends;

// Supply every CoreMIDI symbol used by the processor; do not link CoreMIDI.
const CFStringRef kMIDIPropertyName = CFSTR("name");
ItemCount MIDIGetNumberOfDestinations() {
  return destinations.size();
}
MIDIEndpointRef MIDIGetDestination(ItemCount index) {
  return destinations.at(index);
}
OSStatus MIDIClientCreate(CFStringRef, MIDINotifyProc, void*, MIDIClientRef* client) {
  *client = 1;
  return noErr;
}
OSStatus MIDIOutputPortCreate(MIDIClientRef, CFStringRef, MIDIPortRef* port) {
  *port = 2;
  return noErr;
}
OSStatus MIDIObjectGetStringProperty(MIDIObjectRef, CFStringRef, CFStringRef* name) {
  *name = nullptr;
  return noErr;
}
OSStatus MIDIClientDispose(MIDIClientRef) {
  return noErr;
}
OSStatus MIDIPortDispose(MIDIPortRef) {
  return noErr;
}
MIDIPacket* MIDIPacketListInit(MIDIPacketList* list) {
  return &list->packet[0];
}
MIDIPacket* MIDIPacketListAdd(MIDIPacketList*, ByteCount, MIDIPacket* packet, MIDITimeStamp, ByteCount, const Byte*) {
  return packet;
}
OSStatus MIDISend(MIDIPortRef, MIDIEndpointRef destination, const MIDIPacketList*) {
  sends.push_back(destination);
  return noErr;
}

class PortReply : public ComponentBase {
 public:
  int64 index = -2;
  int count = 0;

  tresult PLUGIN_API notify(IMessage* message) override {
    assert(strcmp(message->getMessageID(), "MIDIPort") == 0);
    assert(message->getAttributes()->getInt("index", index) == kResultOk);
    count++;
    return kResultOk;
  }
};

static void query(tram8::Processor& processor, PortReply& reply, int64 expected) {
  HostMessage message;
  message.setMessageID("GetMIDIPort");
  const size_t sendCount = sends.size();
  const int replyCount = reply.count;
  assert(processor.notify(&message) == kResultOk);
  assert(reply.count == replyCount + 1);
  assert(reply.index == expected);
  assert(sends.size() == sendCount); // Enumeration must not send or resync MIDI.
}

int main() {
  HostApplication host;
  PortReply reply;
  tram8::Processor processor;
  assert(processor.initialize(&host) == kResultOk);
  assert(processor.connect(&reply) == kResultOk);
  assert(sends.size() == 1 && sends.back() == destinations[0]);
  query(processor, reply, 0); // Report the existing processor default.

  for (int64 index : {1, -1, 0}) {
    assert(processor.setActive(false) == kResultOk);
    HostMessage message;
    message.setMessageID("SetMIDIPort");
    message.getAttributes()->setInt("index", index);
    assert(processor.notify(&message) == kResultOk);
    assert(reply.index == index);
    query(processor, reply, index); // Accepted intent is visible while inactive.
    sends.clear();
    for (int reopen = 0; reopen < 2; reopen++) {
      assert(processor.disconnect(&reply) == kResultOk);
      assert(processor.connect(&reply) == kResultOk);
      query(processor, reply, index);
      assert(processor.setActive(true) == kResultOk);
      if (index < 0)
        assert(sends.empty());
      else
        assert(sends.back() == destinations[index]);
    }
  }

  // Find the selected endpoint in the current list rather than caching its index.
  std::swap(destinations[0], destinations[1]);
  query(processor, reply, 1);
  assert(processor.setActive(true) == kResultOk);
  assert(sends.back() == destinations[1]);

  HostMessage invalid;
  invalid.setMessageID("SetMIDIPort");
  invalid.getAttributes()->setInt("index", 99);
  const size_t sendCount = sends.size();
  assert(processor.notify(&invalid) == kResultOk);
  assert(reply.index == 1);
  assert(sends.size() == sendCount);
  HostMessage missingIndex;
  missingIndex.setMessageID("SetMIDIPort");
  assert(processor.notify(&missingIndex) == kResultOk);
  assert(reply.index == 1);
  assert(sends.size() == sendCount);
  assert(processor.terminate() == kResultOk);

  destinations.clear();
  sends.clear();
  tram8::Processor noPorts;
  assert(noPorts.initialize(&host) == kResultOk);
  assert(noPorts.connect(&reply) == kResultOk);
  query(noPorts, reply, -1);
  assert(sends.empty());

  float left[] = {3.f, 1.f, -1.f, 4.f, 5.f, 7.f};
  float right[] = {3.f, -1.f, 2.f, -4.f, 5.f, 7.f};
  float* channels[] = {left + 1, right + 1};
  AudioBusBuffers output{};
  output.numChannels = 2;
  output.channelBuffers32 = channels;
  ProcessData data{};
  data.numOutputs = 1;
  data.outputs = &output;
  data.numSamples = 4;
  assert(noPorts.process(data) == kResultOk);
  for (auto buffer : {std::span{left}, std::span{right}}) {
    assert(buffer.front() == 3.f && buffer.back() == 7.f);
    assert(std::ranges::all_of(buffer.subspan(1, 4), [](float sample) { return sample == 0.f; }));
  }
  data.numSamples = 0;
  output.channelBuffers32 = nullptr;
  assert(noPorts.process(data) == kResultOk);
  assert(sends.empty());
  assert(noPorts.terminate() == kResultOk);
  puts("Processor MIDI port query, output preservation and audio clearing regressions passed");
}
