#include "processor.h"
#include "../../protocol/tram8_sysex.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;

struct SentPacket {
  MIDIEndpointRef destination;
  std::vector<uint8_t> bytes;
};
static std::vector<SentPacket> sent;

// Replace CoreMIDI entry points so this test never sends to attached hardware.
OSStatus MIDIClientCreate(CFStringRef, MIDINotifyProc, void*, MIDIClientRef* client) {
  *client = 1;
  return noErr;
}
OSStatus MIDIOutputPortCreate(MIDIClientRef, CFStringRef, MIDIPortRef* port) {
  *port = 2;
  return noErr;
}
ItemCount MIDIGetNumberOfDestinations() {
  return 2;
}
MIDIEndpointRef MIDIGetDestination(ItemCount index) {
  return 10 + index;
}
OSStatus MIDIObjectGetStringProperty(MIDIObjectRef, CFStringRef, CFStringRef* name) {
  *name = nullptr;
  return noErr;
}
OSStatus MIDIPortDispose(MIDIPortRef) {
  return noErr;
}
OSStatus MIDIClientDispose(MIDIClientRef) {
  return noErr;
}
OSStatus MIDISend(MIDIPortRef, MIDIEndpointRef destination, const MIDIPacketList* packets) {
  const MIDIPacket* packet = &packets->packet[0];
  for (UInt32 i = 0; i < packets->numPackets; ++i) {
    sent.push_back({destination, {packet->data, packet->data + packet->length}});
    packet = MIDIPacketNext(packet);
  }
  return noErr;
}

int main() {
  HostApplication host;
  tram8::Processor processor;
  assert(processor.initialize(&host) == kResultOk);
  sent.clear();

  HostMessage select;
  select.setMessageID("SetMIDIPort");
  auto selectPort = [&](int index) {
    select.getAttributes()->setInt("index", index);
    assert(processor.notify(&select) == kResultOk);
  };
  ProcessData block{};
  block.numSamples = 64;
  selectPort(1);
  assert(sent.empty());
  assert(processor.process(block) == kResultOk);
  assert(sent.size() == 1 && sent[0].destination == 11);
  assert(sent[0].bytes.size() == TRAM8_LEN_COARSE);
  sent.clear();
  processor.process(block);
  assert(sent.empty());

  // Latest selection wins, including disabling output before the next block.
  selectPort(0);
  selectPort(-1);
  processor.process(block);
  assert(sent.empty());

  EventList events;
  Event note{};
  note.type = Event::kNoteOnEvent;
  note.noteOn.channel = 0;
  note.noteOn.pitch = 60;
  note.noteOn.velocity = 1.f;
  events.addEvent(note);
  block.inputEvents = &events;
  processor.process(block);
  assert(sent.empty());

  // Re-enabling output synchronizes held notes even with no new MIDI events.
  block.inputEvents = nullptr;
  selectPort(0);
  assert(sent.empty());
  processor.process(block);
  assert(sent.size() == 1 && sent[0].destination == 10);
  uint8_t gates;
  uint16_t dac[TRAM8_NUM_GATES];
  tram8_form_t form;
  assert(tram8_parse(sent[0].bytes.data(), sent[0].bytes.size(), &gates, dac, &form) == 0);
  assert(gates == 1 && dac[0] == (127 << 5));
  sent.clear();
  processor.process(block);
  assert(sent.empty());

  selectPort(1);
  assert(processor.setActive(true) == kResultOk);
  assert(sent.size() == 1 && sent[0].destination == 11);
  processor.setActive(false);
  processor.terminate();
  std::puts("MIDI port handoff tests passed.");
}
