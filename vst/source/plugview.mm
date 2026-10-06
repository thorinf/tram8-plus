#include "plugview.h"
#include "cids.h"
#include "controller.h"
#include "midi_engine.h"
#include "ui_html.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

#include <cmath>
#include <limits>

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <CoreMIDI/CoreMIDI.h>

using namespace Steinberg;
using namespace Steinberg::Vst;

// ─── Bridge: WKWebView message handler ──────────────────────────────────

namespace tram8 {
class PlugView;
}

@interface Tram8WebBridge : NSObject <WKScriptMessageHandler>
@property(assign) EditController* controller;
@property(assign) WKWebView* webView;
@property(assign) tram8::PlugView* plugView;
@end

static bool ReadInteger(NSDictionary* body, NSString* key, int min, int max, int& result) {
  id value = body[key];
  if (![value isKindOfClass:[NSNumber class]] || CFGetTypeID((CFTypeRef)value) == CFBooleanGetTypeID())
    return false;
  double number = [value doubleValue];
  if (!std::isfinite(number) || number < min || number > max || std::trunc(number) != number)
    return false;
  result = (int)number;
  return true;
}

static void EvaluateNativeEvent(WKWebView* webView, NSDictionary* event) {
  if (!webView || ![NSJSONSerialization isValidJSONObject:event])
    return;

  NSData* json = [NSJSONSerialization dataWithJSONObject:event options:0 error:nil];
  if (!json)
    return;

  NSString* jsonStr = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
  if (!jsonStr)
    return;

  NSString* js = [NSString stringWithFormat:@"typeof tram8 !== 'undefined' && tram8.handleNativeEvent(%@)", jsonStr];
  [webView evaluateJavaScript:js completionHandler:nil];
  [jsonStr release];
}

@implementation Tram8WebBridge

- (void)userContentController:(WKUserContentController*)uc didReceiveScriptMessage:(WKScriptMessage*)message {
  if (!_controller || !_webView || ![message.body isKindOfClass:[NSDictionary class]])
    return;

  NSDictionary* body = message.body;
  NSString* type = body[@"type"];
  if (![type isKindOfClass:[NSString class]])
    return;

  if ([type isEqualToString:@"ready"]) {
    [self pushMidiPorts];
    [self pushState];
    return;
  }

  if ([type isEqualToString:@"setChannel"]) {
    int gate, channel;
    if (!ReadInteger(body, @"gate", 0, tram8::kNumGates - 1, gate) || !ReadInteger(body, @"channel", -1, 15, channel))
      return;
    int step = (channel == -1) ? 0 : (channel + 1);
    double norm = step / 16.0;
    ParamID pid = tram8::kGateChannelBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setNote"]) {
    int gate, note;
    if (!ReadInteger(body, @"gate", 0, tram8::kNumGates - 1, gate) || !ReadInteger(body, @"note", -1, 127, note))
      return;
    int step = (note == -1) ? 0 : (note + 1);
    double norm = step / 128.0;
    ParamID pid = tram8::kGateNoteBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setDacMode"]) {
    int gate, mode;
    if (!ReadInteger(body, @"gate", 0, tram8::kNumGates - 1, gate) ||
        !ReadInteger(body, @"mode", 0, tram8::kDacModeCount - 1, mode))
      return;
    double norm = mode / (double)(tram8::kDacModeCount - 1);
    ParamID pid = tram8::kDacModeBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setDacChannel"]) {
    int gate, channel;
    if (!ReadInteger(body, @"gate", 0, tram8::kNumGates - 1, gate) || !ReadInteger(body, @"channel", -1, 15, channel))
      return;
    int step = (channel == -1) ? 0 : (channel + 1);
    double norm = step / 16.0;
    ParamID pid = tram8::kDacChannelBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setCcNum"]) {
    int gate, cc;
    if (!ReadInteger(body, @"gate", 0, tram8::kNumGates - 1, gate) || !ReadInteger(body, @"cc", 0, 127, cc))
      return;
    double norm = cc / 127.0;
    ParamID pid = tram8::kCcNumBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setMidiPort"]) {
    int index;
    if (!ReadInteger(body, @"index", -1, std::numeric_limits<int>::max(), index) ||
        (index >= 0 && (ItemCount)index >= MIDIGetNumberOfDestinations()))
      return;
    if (auto* msg = _controller->allocateMessage()) {
      msg->setMessageID("SetMIDIPort");
      msg->getAttributes()->setInt("index", (Steinberg::int64)index);
      _controller->sendMessage(msg);
      msg->release();
    }
    return;
  }

  if ([type isEqualToString:@"resize"]) {
    int height;
    if (!ReadInteger(body, @"height", 1, std::numeric_limits<int>::max(), height))
      return;
    NSLog(@"tram8+: JS resize request height=%d", height);
    if (_plugView)
      _plugView->resizeTo(560, height);
    return;
  }
}

- (void)pushMidiPorts {
  NSMutableArray* ports = [NSMutableArray array];
  ItemCount destCount = MIDIGetNumberOfDestinations();
  for (ItemCount i = 0; i < destCount; i++) {
    MIDIEndpointRef ep = MIDIGetDestination(i);
    CFStringRef name = NULL;
    MIDIObjectGetStringProperty(ep, kMIDIPropertyName, &name);
    if (name) {
      [ports addObject:(__bridge NSString*)name];
      CFRelease(name);
    } else {
      [ports addObject:[NSString stringWithFormat:@"Port %lu", i]];
    }
  }
  EvaluateNativeEvent(_webView, @{@"type" : @"midiPorts", @"ports" : ports});
}

- (void)pushState {
  NSMutableArray* gates = [NSMutableArray arrayWithCapacity:8];
  for (int i = 0; i < 8; i++) {
    double chNorm = _controller->getParamNormalized(tram8::kGateChannelBase + i);
    int chStep = (int)(chNorm * 16 + 0.5);
    int channel = (chStep == 0) ? -1 : (chStep - 1);

    double noteNorm = _controller->getParamNormalized(tram8::kGateNoteBase + i);
    int noteStep = (int)(noteNorm * 128 + 0.5);
    int note = (noteStep == 0) ? -1 : (noteStep - 1);

    double modeNorm = _controller->getParamNormalized(tram8::kDacModeBase + i);
    int mode = (int)(modeNorm * (tram8::kDacModeCount - 1) + 0.5);

    double dacChNorm = _controller->getParamNormalized(tram8::kDacChannelBase + i);
    int dacChStep = (int)(dacChNorm * 16 + 0.5);
    int dacCh = (dacChStep == 0) ? -1 : (dacChStep - 1);

    double ccNorm = _controller->getParamNormalized(tram8::kCcNumBase + i);
    int ccN = (int)(ccNorm * 127 + 0.5);

    [gates addObject:@{
      @"gate" : @(i),
      @"channel" : @(channel),
      @"note" : @(note),
      @"mode" : @(mode),
      @"dacChannel" : @(dacCh),
      @"ccNum" : @(ccN)
    }];
  }
  EvaluateNativeEvent(_webView, @{@"type" : @"state", @"gates" : gates});
}

@end

// ─── PlugView implementation ─────────────────────────────────────────────

namespace tram8 {

PlugView::PlugView(EditController* ctrl) : controller(ctrl) {
  controller->addRef();
}

PlugView::~PlugView() {
  removed();
  controller->release();
}

tresult PLUGIN_API PlugView::isPlatformTypeSupported(FIDString type) {
  if (strcmp(type, kPlatformTypeNSView) == 0)
    return kResultOk;
  return kResultFalse;
}

tresult PLUGIN_API PlugView::attached(void* parent, FIDString type) {
  if (strcmp(type, kPlatformTypeNSView) != 0)
    return kResultFalse;

  if (!parent)
    return kInvalidArgument;
  removed();

  NSView* parentView = (__bridge NSView*)parent;

  bridge = [[Tram8WebBridge alloc] init];
  bridge.controller = controller;
  bridge.plugView = this;

  WKWebViewConfiguration* config = [[[WKWebViewConfiguration alloc] init] autorelease];
  [config.userContentController addScriptMessageHandler:bridge name:@"tram8"];

  NSRect frame = NSMakeRect(0, 0, kWidth, currentHeight);
  webView = [[WKWebView alloc] initWithFrame:frame configuration:config];
  bridge.webView = webView;

  [webView setValue:@NO forKey:@"drawsBackground"];

  NSString* html = [NSString stringWithUTF8String:kUIHTML];
  [webView loadHTMLString:html baseURL:nil];

  [parentView addSubview:webView];
  static_cast<Controller*>(controller)->setActiveView(this);
  return kResultOk;
}

tresult PLUGIN_API PlugView::removed() {
  static_cast<Controller*>(controller)->clearActiveView(this);
  bridge.controller = nullptr;
  bridge.plugView = nullptr;
  bridge.webView = nil;
  if (webView) {
    [webView.configuration.userContentController removeScriptMessageHandlerForName:@"tram8"];
    [webView removeFromSuperview];
    [webView release];
    webView = nullptr;
  }
  [bridge release];
  bridge = nullptr;
  return kResultOk;
}

tresult PLUGIN_API PlugView::getSize(ViewRect* size) {
  if (!size)
    return kResultFalse;
  size->left = 0;
  size->top = 0;
  size->right = kWidth;
  size->bottom = currentHeight;
  return kResultOk;
}

tresult PLUGIN_API PlugView::onSize(ViewRect* newSize) {
  if (!newSize || !webView)
    return kResultOk;
  int w = newSize->right - newSize->left;
  int h = newSize->bottom - newSize->top;
  NSLog(@"tram8+: onSize w=%d h=%d", w, h);
  [webView setFrame:NSMakeRect(0, 0, w, h)];
  return kResultOk;
}

void PlugView::resizeTo(int width, int height) {
  if (height < kMinHeight)
    height = kMinHeight;
  if (height > kMaxHeight)
    height = kMaxHeight;
  NSLog(@"tram8+: resizeTo w=%d h=%d (current=%d, plugFrame=%p)", width, height, currentHeight, plugFrame);
  if (height == currentHeight)
    return;

  if (plugFrame) {
    ViewRect rect = {0, 0, (int32)width, (int32)height};
    tresult r = plugFrame->resizeView(this, &rect);
    NSLog(@"tram8+: resizeView result=%d", r);
  }

  currentHeight = height;
  if (webView) {
    [webView setFrame:NSMakeRect(0, 0, width, height)];
  }
}

tresult PLUGIN_API PlugView::setFrame(IPlugFrame* frame) {
  plugFrame = frame;
  return kResultOk;
}

tresult PLUGIN_API PlugView::checkSizeConstraint(ViewRect* rect) {
  if (!rect)
    return kResultFalse;
  rect->left = 0;
  rect->top = 0;
  rect->right = kWidth;
  if (rect->bottom < kMinHeight)
    rect->bottom = kMinHeight;
  if (rect->bottom > kMaxHeight)
    rect->bottom = kMaxHeight;
  return kResultOk;
}

tresult PLUGIN_API PlugView::queryInterface(const TUID iid, void** obj) {
  if (FUnknownPrivate::iidEqual(iid, IPlugView::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
    addRef();
    *obj = static_cast<IPlugView*>(this);
    return kResultOk;
  }
  *obj = nullptr;
  return kNoInterface;
}

uint32 PLUGIN_API PlugView::addRef() {
  return refCount.fetch_add(1, std::memory_order_relaxed) + 1;
}

uint32 PLUGIN_API PlugView::release() {
  uint32 prev;
  {
    // Serialize the final release with Controller::notify's temporary reference.
    auto* ctrl = static_cast<Controller*>(controller);
    std::lock_guard<std::mutex> lock(ctrl->activeViewMutex);
    prev = refCount.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1 && ctrl->activeView == this)
      ctrl->activeView = nullptr;
  }
  if (prev == 1) {
    if ([NSThread isMainThread])
      delete this;
    else {
      PlugView* self = this;
      dispatch_async(dispatch_get_main_queue(), ^{
        delete self;
      });
    }
    return 0;
  }
  return prev - 1;
}

void PlugView::flashMidiInput() {
  PlugView* self = this;
  self->addRef();
  dispatch_async(dispatch_get_main_queue(), ^{
    if (WKWebView* wv = self->webView)
      EvaluateNativeEvent(wv, @{@"type" : @"activity", @"input" : @(YES)});
    self->release();
  });
}

void PlugView::flashMidiOutput() {
  PlugView* self = this;
  self->addRef();
  dispatch_async(dispatch_get_main_queue(), ^{
    if (WKWebView* wv = self->webView)
      EvaluateNativeEvent(wv, @{@"type" : @"activity", @"output" : @(YES)});
    self->release();
  });
}

} // namespace tram8
