#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace tram8 {
static const Steinberg::FUID kProcessorUID(0x1A2B3C4D, 0x5E6F7A8B, 0x9C0D1E2F, 0x3A4B5C6D);
static const Steinberg::FUID kControllerUID(0x7E8F9A0B, 0x1C2D3E4F, 0x5A6B7C8D, 0x9E0F1A2B);

enum ParamIDs : Steinberg::Vst::ParamID {
  kGateChannelBase = 100, // 100-107
  kGateNoteBase = 200,    // 200-207
  kDacModeBase = 300,     // 300-307
  kDacChannelBase = 400,  // 400-407
  kCcNumBase = 500,       // 500-507
  kCcValueBase = 600,     // 600-727 (one per CC 0-127)
};

enum DacMode {
  kDacVelocity = 0,
  kDacPitch = 1,
  kDacCC = 2,
  kDacOff = 3,
  kDacModeCount = 4,
};
} // namespace tram8
