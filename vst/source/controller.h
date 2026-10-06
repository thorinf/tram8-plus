#pragma once

#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include <mutex>

namespace tram8 {

class PlugView;

class Controller : public Steinberg::Vst::EditController, public Steinberg::Vst::IMidiMapping {
 public:
  static Steinberg::FUnknown* createInstance(void*) {
    return static_cast<Steinberg::Vst::IEditController*>(new Controller());
  }

  Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
  Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override;
  Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
  Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) override;

  void setActiveView(PlugView* view) {
    std::lock_guard<std::mutex> lock(activeViewMutex);
    activeView = view;
  }
  void clearActiveView(PlugView* view) {
    std::lock_guard<std::mutex> lock(activeViewMutex);
    if (activeView == view)
      activeView = nullptr;
  }

  Steinberg::tresult PLUGIN_API getMidiControllerAssignment(Steinberg::int32 busIndex,
                                                            Steinberg::int16 channel,
                                                            Steinberg::Vst::CtrlNumber midiControllerNumber,
                                                            Steinberg::Vst::ParamID& id) override;

  OBJ_METHODS(Controller, EditController)
  DEFINE_INTERFACES
  DEF_INTERFACE(IMidiMapping)
  END_DEFINE_INTERFACES(EditController)
  REFCOUNT_METHODS(EditController)

 private:
  friend class PlugView;
  std::mutex activeViewMutex;
  PlugView* activeView = nullptr;
};

} // namespace tram8
