#include "controller.h"
#include "base/source/fstring.h"
#include "cids.h"
#include "pluginterfaces/base/ibstream.h"
#include "plugview.h"
#include "public.sdk/source/vst/vstparameters.h"

#include <cstdio>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace tram8 {

tresult PLUGIN_API Controller::initialize(FUnknown *context) {
  tresult result = EditController::initialize(context);
  if (result != kResultOk) return result;

  const char *noteNamesCh[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

  for (int i = 0; i < 8; i++) {
    auto *chParam = new StringListParameter(STR16("Channel"), kGateChannelBase + i);
    chParam->appendString(STR16("Any"));
    for (int ch = 1; ch <= 16; ch++) {
      char buf[16];
      snprintf(buf, sizeof(buf), "Ch %d", ch);
      String128 s;
      Steinberg::String str(buf);
      str.copyTo16(s, 0, 127);
      chParam->appendString(s);
    }
    parameters.addParameter(chParam);

    auto *noteParam = new StringListParameter(STR16("Note"), kGateNoteBase + i);
    noteParam->appendString(STR16("Any"));
    for (int n = 0; n < 128; n++) {
      int octave = (n / 12) - 1;
      char buf[32];
      snprintf(buf, sizeof(buf), "%s%d (%d)", noteNamesCh[n % 12], octave, n);
      String128 s;
      Steinberg::String str(buf);
      str.copyTo16(s, 0, 127);
      noteParam->appendString(s);
    }
    noteParam->setNormalized(noteParam->toNormalized(61 + i));
    parameters.addParameter(noteParam);

    auto *modeParam = new StringListParameter(STR16("DAC Mode"), kDacModeBase + i);
    modeParam->appendString(STR16("Velocity"));
    modeParam->appendString(STR16("Pitch"));
    modeParam->appendString(STR16("Off"));
    parameters.addParameter(modeParam);
  }

  return kResultOk;
}

IPlugView *PLUGIN_API Controller::createView(FIDString name) {
  if (strcmp(name, ViewType::kEditor) == 0) { return new PlugView(this); }
  return nullptr;
}

tresult PLUGIN_API Controller::setComponentState(IBStream *state) {
  if (!state) return kResultFalse;

  for (int i = 0; i < 8; i++) {
    int32 chVal = 0, noteVal = 0, modeVal = 0;
    if (state->read(&chVal, sizeof(int32)) != kResultOk) break;
    if (state->read(&noteVal, sizeof(int32)) != kResultOk) break;
    if (state->read(&modeVal, sizeof(int32)) != kResultOk) break;

    int chStep = chVal + 1;
    if (chStep < 0) chStep = 0;
    if (chStep > 16) chStep = 16;
    auto *chParam = parameters.getParameter(kGateChannelBase + i);
    if (chParam) chParam->setNormalized(chParam->toNormalized(chStep));

    int noteStep = noteVal + 1;
    if (noteStep < 0) noteStep = 0;
    if (noteStep > 128) noteStep = 128;
    auto *noteParam = parameters.getParameter(kGateNoteBase + i);
    if (noteParam) noteParam->setNormalized(noteParam->toNormalized(noteStep));

    if (modeVal < 0) modeVal = 0;
    if (modeVal >= kDacModeCount) modeVal = 0;
    auto *modeParam = parameters.getParameter(kDacModeBase + i);
    if (modeParam) modeParam->setNormalized(modeParam->toNormalized(modeVal));
  }

  return kResultOk;
}

} // namespace tram8
