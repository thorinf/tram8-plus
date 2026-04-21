#pragma once

#include <cstdint>
#include <cstring>

namespace tram8 {

static constexpr int kNumGates = 8;

enum DacMode {
  kDacVelocity = 0,
  kDacPitch = 1,
  kDacCC = 2,
  kDacOff = 3,
  kDacModeCount = 4,
};

struct NoteEntry {
  int16_t channel = 0;
  int16_t note = 0;
  uint8_t velocity = 0;
};

struct NoteStack {
  static constexpr int kMaxNotes = 16;
  NoteEntry entries[kMaxNotes];
  int count = 0;

  void push(int16_t channel, int16_t note, uint8_t velocity) {
    remove(channel, note);
    if (count < kMaxNotes) {
      entries[count].channel = channel;
      entries[count].note = note;
      entries[count].velocity = velocity;
      count++;
    }
  }

  void remove(int16_t channel, int16_t note) {
    for (int i = 0; i < count; i++) {
      if (entries[i].channel == channel && entries[i].note == note) {
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

class MidiEngine {
 public:
  MidiEngine() { reset(); }

  void setGateChannel(int gate, int8_t channel) { gateChannel_[gate] = channel; }
  void setGateNote(int gate, int16_t note) { gateNote_[gate] = note; }
  void setDacMode(int gate, uint8_t mode) { dacMode_[gate] = mode; }
  void setDacChannel(int gate, int8_t channel) { dacChannel_[gate] = channel; }
  void setCcNum(int gate, uint8_t cc) { ccNum_[gate] = cc; }

  void setCcValue(uint8_t cc, uint8_t value) {
    ccValues_[cc] = value;
    for (int g = 0; g < kNumGates; g++) {
      if (dacMode_[g] == kDacCC && ccNum_[g] == cc && !noteStacks_[g].empty())
        dacValues_[g] = (uint16_t)value << 7;
    }
  }

  void noteOn(int16_t channel, int16_t note, float velocity) {
    if (velocity == 0.f) {
      noteOff(channel, note);
      return;
    }
    uint8_t vel = (uint8_t)(velocity * 127.0f);
    for (int g = 0; g < kNumGates; g++) {
      bool gateChMatch = (gateChannel_[g] == -1) || (gateChannel_[g] == channel);
      bool gateNoteMatch = (gateNote_[g] == -1) || (gateNote_[g] == note);
      if (gateChMatch && gateNoteMatch) {
        gateStacks_[g].push(channel, note, vel);
        gateMask_ |= (1 << g);
      }

      bool dacChMatch = (dacChannel_[g] == -1) || (dacChannel_[g] == channel);
      if (dacChMatch) {
        noteStacks_[g].push(channel, note, vel);
        updateDac(g, note, vel);
      }
    }
  }

  void noteOff(int16_t channel, int16_t note) {
    for (int g = 0; g < kNumGates; g++) {
      bool gateChMatch = (gateChannel_[g] == -1) || (gateChannel_[g] == channel);
      bool gateNoteMatch = (gateNote_[g] == -1) || (gateNote_[g] == note);
      if (gateChMatch && gateNoteMatch) {
        gateStacks_[g].remove(channel, note);
        if (gateStacks_[g].empty())
          gateMask_ &= ~(1 << g);
      }

      bool dacChMatch = (dacChannel_[g] == -1) || (dacChannel_[g] == channel);
      if (dacChMatch) {
        noteStacks_[g].remove(channel, note);
        if (!noteStacks_[g].empty()) {
          updateDac(g, noteStacks_[g].top().note, noteStacks_[g].top().velocity);
        } else if (dacMode_[g] == kDacVelocity) {
          dacValues_[g] = 0;
        }
      }
    }
  }

  uint8_t gateMask() const { return gateMask_; }
  const uint16_t* dacValues() const { return dacValues_; }

  bool stateChanged() const {
    if (gateMask_ != prevGateMask_)
      return true;
    return dacChanged();
  }

  bool dacChanged() const { return memcmp(dacValues_, prevDacValues_, sizeof(dacValues_)) != 0; }

  void markSent() {
    prevGateMask_ = gateMask_;
    memcpy(prevDacValues_, dacValues_, sizeof(dacValues_));
  }

  bool hasPitchMode() const {
    for (int g = 0; g < kNumGates; g++) {
      if (dacMode_[g] == kDacPitch)
        return true;
    }
    return false;
  }

  void clearRuntime() {
    gateMask_ = 0;
    prevGateMask_ = 0;
    memset(dacValues_, 0, sizeof(dacValues_));
    memset(prevDacValues_, 0, sizeof(prevDacValues_));
    for (int i = 0; i < kNumGates; i++) {
      gateStacks_[i].count = 0;
      noteStacks_[i].count = 0;
    }
  }

  void reset() {
    clearRuntime();
    for (int i = 0; i < kNumGates; i++) {
      gateChannel_[i] = -1;
      gateNote_[i] = 60 + i;
      dacMode_[i] = kDacVelocity;
      dacChannel_[i] = -1;
      ccNum_[i] = 1;
    }
    memset(ccValues_, 0, sizeof(ccValues_));
  }

  static constexpr int kStateWordsPerGate = 5;

  void serialize(int32_t* out) const {
    for (int i = 0; i < kNumGates; i++) {
      *out++ = gateChannel_[i];
      *out++ = gateNote_[i];
      *out++ = dacMode_[i];
      *out++ = dacChannel_[i];
      *out++ = ccNum_[i];
    }
  }

  void deserialize(const int32_t* in) {
    for (int i = 0; i < kNumGates; i++) {
      int32_t ch = *in++;
      int32_t note = *in++;
      int32_t mode = *in++;
      int32_t dCh = *in++;
      int32_t ccN = *in++;
      if (ch < -1)
        ch = -1;
      else if (ch > 15)
        ch = 15;
      if (note < -1)
        note = -1;
      else if (note > 127)
        note = 127;
      if (mode < 0 || mode >= kDacModeCount)
        mode = kDacVelocity;
      if (dCh < -1)
        dCh = -1;
      else if (dCh > 15)
        dCh = 15;
      if (ccN < 0)
        ccN = 0;
      else if (ccN > 127)
        ccN = 127;
      gateChannel_[i] = (int8_t)ch;
      gateNote_[i] = (int16_t)note;
      dacMode_[i] = (uint8_t)mode;
      dacChannel_[i] = (int8_t)dCh;
      ccNum_[i] = (uint8_t)ccN;
    }
  }

  static const uint16_t pitchLookup[61];

 private:
  int8_t gateChannel_[kNumGates];
  int16_t gateNote_[kNumGates];
  uint8_t dacMode_[kNumGates];
  int8_t dacChannel_[kNumGates];
  uint8_t ccNum_[kNumGates];
  uint8_t ccValues_[128];

  NoteStack gateStacks_[kNumGates];
  NoteStack noteStacks_[kNumGates];
  uint8_t gateMask_;
  uint16_t dacValues_[kNumGates];
  uint8_t prevGateMask_;
  uint16_t prevDacValues_[kNumGates];

  void updateDac(int g, int16_t note, uint8_t velocity) {
    switch (dacMode_[g]) {
      case kDacPitch: {
        int n = note;
        if (n < 0)
          n = 0;
        if (n > 60)
          n = 60;
        dacValues_[g] = (pitchLookup[n] >> 2) & 0x3FFC;
        break;
      }
      case kDacCC:
        dacValues_[g] = (uint16_t)ccValues_[ccNum_[g]] << 7;
        break;
      case kDacOff:
        break;
      default:
        dacValues_[g] = (uint16_t)velocity << 7;
        break;
    }
  }
};

} // namespace tram8
