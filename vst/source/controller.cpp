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

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
  tresult result = EditController::initialize(context);
  if (result != kResultOk)
    return result;

  const char* noteNamesCh[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

  for (int i = 0; i < 8; i++) {
    auto* chParam = new StringListParameter(STR16("Channel"), kGateChannelBase + i);
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

    auto* noteParam = new StringListParameter(STR16("Note"), kGateNoteBase + i);
    noteParam->appendString(STR16("Any"));
    for (int n = 0; n < 128; n++) {
      int octave = (n / 12) - 2;
      char buf[32];
      snprintf(buf, sizeof(buf), "%s%d (%d)", noteNamesCh[n % 12], octave, n);
      String128 s;
      Steinberg::String str(buf);
      str.copyTo16(s, 0, 127);
      noteParam->appendString(s);
    }
    noteParam->setNormalized(noteParam->toNormalized(61 + i));
    parameters.addParameter(noteParam);

    auto* modeParam = new StringListParameter(STR16("DAC Mode"), kDacModeBase + i);
    modeParam->appendString(STR16("Velocity"));
    modeParam->appendString(STR16("Pitch"));
    modeParam->appendString(STR16("CC"));
    modeParam->appendString(STR16("Off"));
    parameters.addParameter(modeParam);

    auto* dacChParam = new StringListParameter(STR16("DAC Channel"), kDacChannelBase + i);
    dacChParam->appendString(STR16("Any"));
    for (int ch = 1; ch <= 16; ch++) {
      char buf[16];
      snprintf(buf, sizeof(buf), "Ch %d", ch);
      String128 s;
      Steinberg::String str(buf);
      str.copyTo16(s, 0, 127);
      dacChParam->appendString(s);
    }
    parameters.addParameter(dacChParam);

    auto* ccParam = new StringListParameter(STR16("CC Number"), kCcNumBase + i);
    for (int cc = 0; cc < 128; cc++) {
      char buf[16];
      snprintf(buf, sizeof(buf), "CC %d", cc);
      String128 s;
      Steinberg::String str(buf);
      str.copyTo16(s, 0, 127);
      ccParam->appendString(s);
    }
    ccParam->setNormalized(ccParam->toNormalized(1));
    parameters.addParameter(ccParam);
  }

  for (int cc = 0; cc < 128; cc++) {
    char buf[16];
    snprintf(buf, sizeof(buf), "CC %d Value", cc);
    String128 s;
    Steinberg::String str(buf);
    str.copyTo16(s, 0, 127);
    auto* p = parameters.addParameter(s, nullptr, 0, 0, ParameterInfo::kIsHidden, kCcValueBase + cc);
    (void)p;
  }

  return kResultOk;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
  if (strcmp(name, ViewType::kEditor) == 0) {
    return new PlugView(this);
  }
  return nullptr;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
  if (!state)
    return kResultFalse;

  for (int i = 0; i < 8; i++) {
    int32 chVal = 0, noteVal = 0, modeVal = 0, dacChVal = -1, ccNumVal = 1;
    if (state->read(&chVal, sizeof(int32)) != kResultOk)
      break;
    if (state->read(&noteVal, sizeof(int32)) != kResultOk)
      break;
    if (state->read(&modeVal, sizeof(int32)) != kResultOk)
      break;
    if (state->read(&dacChVal, sizeof(int32)) != kResultOk)
      dacChVal = -1;
    if (state->read(&ccNumVal, sizeof(int32)) != kResultOk)
      ccNumVal = 1;

    int chStep = chVal + 1;
    if (chStep < 0)
      chStep = 0;
    if (chStep > 16)
      chStep = 16;
    auto* chParam = parameters.getParameter(kGateChannelBase + i);
    if (chParam)
      chParam->setNormalized(chParam->toNormalized(chStep));

    int noteStep = noteVal + 1;
    if (noteStep < 0)
      noteStep = 0;
    if (noteStep > 128)
      noteStep = 128;
    auto* noteParam = parameters.getParameter(kGateNoteBase + i);
    if (noteParam)
      noteParam->setNormalized(noteParam->toNormalized(noteStep));

    if (modeVal < 0)
      modeVal = 0;
    if (modeVal >= kDacModeCount)
      modeVal = 0;
    auto* modeParam = parameters.getParameter(kDacModeBase + i);
    if (modeParam)
      modeParam->setNormalized(modeParam->toNormalized(modeVal));

    int dacChStep = dacChVal + 1;
    if (dacChStep < 0)
      dacChStep = 0;
    if (dacChStep > 16)
      dacChStep = 16;
    auto* dacChParam = parameters.getParameter(kDacChannelBase + i);
    if (dacChParam)
      dacChParam->setNormalized(dacChParam->toNormalized(dacChStep));

    if (ccNumVal < 0)
      ccNumVal = 0;
    if (ccNumVal > 127)
      ccNumVal = 127;
    auto* ccParam = parameters.getParameter(kCcNumBase + i);
    if (ccParam)
      ccParam->setNormalized(ccParam->toNormalized(ccNumVal));
  }

  return kResultOk;
}

tresult PLUGIN_API Controller::getMidiControllerAssignment(int32 /*busIndex*/,
                                                           int16 /*channel*/,
                                                           CtrlNumber midiControllerNumber,
                                                           ParamID& id) {
  if (midiControllerNumber >= 0 && midiControllerNumber < 128) {
    id = kCcValueBase + midiControllerNumber;
    return kResultOk;
  }
  return kResultFalse;
}

} // namespace tram8
