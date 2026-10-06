#pragma once

#include "midi_engine.h"
#include "pluginterfaces/base/ibstream.h"

#include <cstdint>

namespace tram8 {

static constexpr int32_t kStateMagic = 0x54523853; // "TR8S"
static constexpr int32_t kStateVersion = 1;
static constexpr int kLegacyStateWordsPerGate = 3;

inline bool streamReadInt32(Steinberg::IBStream* stream, int32_t& value) {
  Steinberg::int32 bytesRead = 0;
  return stream->read(&value, sizeof(value), &bytesRead) == Steinberg::kResultOk && bytesRead == sizeof(value);
}

inline bool streamWriteInt32(Steinberg::IBStream* stream, int32_t value) {
  Steinberg::int32 bytesWritten = 0;
  return stream->write(&value, sizeof(value), &bytesWritten) == Steinberg::kResultOk && bytesWritten == sizeof(value);
}

inline bool streamRemainingBytes(Steinberg::IBStream* stream, Steinberg::int64& remaining) {
  Steinberg::int64 start = 0;
  Steinberg::int64 end = 0;
  if (stream->tell(&start) != Steinberg::kResultOk)
    return false;
  if (stream->seek(0, Steinberg::IBStream::kIBSeekEnd, &end) != Steinberg::kResultOk)
    return false;
  if (stream->seek(start, Steinberg::IBStream::kIBSeekSet) != Steinberg::kResultOk)
    return false;
  remaining = end - start;
  return remaining >= 0;
}

inline bool readLegacyStateWords(Steinberg::IBStream* stream,
                                 int32_t firstWord,
                                 int wordsPerGate,
                                 std::span<int32_t, MidiEngine::kStateWordCount> out) {
  for (int gate = 0; gate < kNumGates; gate++) {
    int32_t ch = firstWord;
    if (gate != 0 && !streamReadInt32(stream, ch))
      return false;

    int32_t note = 0;
    int32_t mode = 0;
    int32_t dacChannel = -1;
    int32_t ccNumber = 1;
    if (!streamReadInt32(stream, note) || !streamReadInt32(stream, mode))
      return false;
    if (wordsPerGate == MidiEngine::kStateWordsPerGate &&
        (!streamReadInt32(stream, dacChannel) || !streamReadInt32(stream, ccNumber))) {
      return false;
    }

    int off = gate * MidiEngine::kStateWordsPerGate;
    out[off + 0] = ch;
    out[off + 1] = note;
    out[off + 2] = mode;
    out[off + 3] = dacChannel;
    out[off + 4] = ccNumber;
  }
  return true;
}

inline bool readStateWords(Steinberg::IBStream* stream, std::span<int32_t, MidiEngine::kStateWordCount> out) {
  Steinberg::int64 remaining = 0;
  bool hasSize = streamRemainingBytes(stream, remaining);

  int32_t first = 0;
  if (!streamReadInt32(stream, first))
    return false;

  constexpr Steinberg::int64 kVersionedBytes =
      (2 + kNumGates * MidiEngine::kStateWordsPerGate) * (Steinberg::int64)sizeof(int32_t);
  constexpr Steinberg::int64 kLegacyBytes =
      (kNumGates * MidiEngine::kStateWordsPerGate) * (Steinberg::int64)sizeof(int32_t);
  constexpr Steinberg::int64 kLegacy3Bytes = (kNumGates * kLegacyStateWordsPerGate) * (Steinberg::int64)sizeof(int32_t);

  if (first == kStateMagic) {
    if (hasSize && remaining != kVersionedBytes)
      return false;
    int32_t version = 0;
    if (!streamReadInt32(stream, version) || version != kStateVersion)
      return false;
    for (auto& word : out) {
      if (!streamReadInt32(stream, word))
        return false;
    }
    return true;
  }

  if (hasSize) {
    if (remaining == kLegacyBytes)
      return readLegacyStateWords(stream, first, MidiEngine::kStateWordsPerGate, out);
    if (remaining == kLegacy3Bytes)
      return readLegacyStateWords(stream, first, kLegacyStateWordsPerGate, out);
    return false;
  }

  return readLegacyStateWords(stream, first, MidiEngine::kStateWordsPerGate, out);
}

inline bool writeStateWords(Steinberg::IBStream* stream, std::span<const int32_t, MidiEngine::kStateWordCount> words) {
  if (!streamWriteInt32(stream, kStateMagic) || !streamWriteInt32(stream, kStateVersion))
    return false;
  for (auto word : words) {
    if (!streamWriteInt32(stream, word))
      return false;
  }
  return true;
}

} // namespace tram8
