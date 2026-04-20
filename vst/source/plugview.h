#pragma once

#include <atomic>

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/gui/iplugview.h"

#ifdef __APPLE__
#ifdef __OBJC__
@class WKWebView;
@class Tram8WebBridge;
#else
typedef void WKWebView;
typedef void Tram8WebBridge;
#endif
#endif

namespace Steinberg {
namespace Vst {
class EditController;
}
} // namespace Steinberg

namespace tram8 {

class PlugView : public Steinberg::IPlugView {
 public:
  PlugView(Steinberg::Vst::EditController* controller);
  virtual ~PlugView();

  Steinberg::tresult PLUGIN_API isPlatformTypeSupported(Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API attached(void* parent, Steinberg::FIDString type) override;
  Steinberg::tresult PLUGIN_API removed() override;
  Steinberg::tresult PLUGIN_API onWheel(float) override { return Steinberg::kResultFalse; }
  Steinberg::tresult PLUGIN_API onKeyDown(Steinberg::char16, Steinberg::int16, Steinberg::int16) override {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API onKeyUp(Steinberg::char16, Steinberg::int16, Steinberg::int16) override {
    return Steinberg::kResultFalse;
  }
  Steinberg::tresult PLUGIN_API getSize(Steinberg::ViewRect* size) override;
  Steinberg::tresult PLUGIN_API onSize(Steinberg::ViewRect* newSize) override;
  Steinberg::tresult PLUGIN_API onFocus(Steinberg::TBool) override { return Steinberg::kResultFalse; }
  Steinberg::tresult PLUGIN_API setFrame(Steinberg::IPlugFrame* frame) override;
  Steinberg::tresult PLUGIN_API canResize() override { return Steinberg::kResultTrue; }
  Steinberg::tresult PLUGIN_API checkSizeConstraint(Steinberg::ViewRect* rect) override;

  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID iid, void** obj) override;
  Steinberg::uint32 PLUGIN_API addRef() override;
  Steinberg::uint32 PLUGIN_API release() override;

  void resizeTo(int width, int height);
  void flashMidiInput();
  void flashMidiOutput();

 private:
  std::atomic<Steinberg::uint32> refCount = 1;
  Steinberg::IPlugFrame* plugFrame = nullptr;
  Steinberg::Vst::EditController* controller = nullptr;
  WKWebView* webView = nullptr;
  Tram8WebBridge* bridge = nullptr;

  static constexpr int kWidth = 560;
  int currentHeight = 300;
  static constexpr int kMinHeight = 300;
  static constexpr int kMaxHeight = 800;
};

} // namespace tram8
