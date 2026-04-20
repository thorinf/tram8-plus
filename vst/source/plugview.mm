#include "plugview.h"
#include "cids.h"
#include "controller.h"
#include "ui_html.h"
#include "pluginterfaces/gui/iplugview.h"
#include "public.sdk/source/vst/vsteditcontroller.h"

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

@implementation Tram8WebBridge

- (void)userContentController:(WKUserContentController*)uc didReceiveScriptMessage:(WKScriptMessage*)message {
  NSDictionary* body = message.body;
  NSString* type = body[@"type"];

  if ([type isEqualToString:@"ready"]) {
    [self pushMidiPorts];
    [self pushState];
    return;
  }

  if ([type isEqualToString:@"setChannel"]) {
    int gate = [body[@"gate"] intValue];
    int channel = [body[@"channel"] intValue];
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
    int gate = [body[@"gate"] intValue];
    int note = [body[@"note"] intValue];
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
    int gate = [body[@"gate"] intValue];
    int mode = [body[@"mode"] intValue];
    double norm = mode / (double)(tram8::kDacModeCount - 1);
    ParamID pid = tram8::kDacModeBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setDacChannel"]) {
    int gate = [body[@"gate"] intValue];
    int channel = [body[@"channel"] intValue];
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
    int gate = [body[@"gate"] intValue];
    int cc = [body[@"cc"] intValue];
    double norm = cc / 127.0;
    ParamID pid = tram8::kCcNumBase + gate;
    _controller->beginEdit(pid);
    _controller->performEdit(pid, norm);
    _controller->setParamNormalized(pid, norm);
    _controller->endEdit(pid);
    return;
  }

  if ([type isEqualToString:@"setMidiPort"]) {
    int index = [body[@"index"] intValue];
    if (auto* msg = _controller->allocateMessage()) {
      msg->setMessageID("SetMIDIPort");
      msg->getAttributes()->setInt("index", (Steinberg::int64)index);
      _controller->sendMessage(msg);
      msg->release();
    }
    return;
  }

  if ([type isEqualToString:@"resize"]) {
    int height = [body[@"height"] intValue];
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
  NSData* json = [NSJSONSerialization dataWithJSONObject:ports options:0 error:nil];
  NSString* jsonStr = [[NSString alloc] initWithData:json encoding:NSUTF8StringEncoding];
  NSString* js = [NSString stringWithFormat:@"tram8.setMidiPorts(%@)", jsonStr];
  [_webView evaluateJavaScript:js completionHandler:nil];
  [jsonStr release];
}

- (void)pushState {
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

    NSString* js =
        [NSString stringWithFormat:@"tram8.setGateState(%d, %d, %d, %d, %d, %d)", i, channel, note, mode, dacCh, ccN];
    [_webView evaluateJavaScript:js completionHandler:nil];
  }
}

@end

// ─── PlugView implementation ─────────────────────────────────────────────

namespace tram8 {

PlugView::PlugView(EditController* ctrl) : controller(ctrl) {}

PlugView::~PlugView() {
  if (webView) {
    [webView.configuration.userContentController removeScriptMessageHandlerForName:@"tram8"];
    [webView removeFromSuperview];
    [webView release];
    webView = nullptr;
  }
  [bridge release];
  bridge = nullptr;
}

tresult PLUGIN_API PlugView::isPlatformTypeSupported(FIDString type) {
  if (strcmp(type, kPlatformTypeNSView) == 0)
    return kResultOk;
  return kResultFalse;
}

tresult PLUGIN_API PlugView::attached(void* parent, FIDString type) {
  if (strcmp(type, kPlatformTypeNSView) != 0)
    return kResultFalse;

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
  return kResultOk;
}

tresult PLUGIN_API PlugView::removed() {
  static_cast<Controller*>(controller)->setActiveView(nullptr);
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
  return ++refCount;
}

uint32 PLUGIN_API PlugView::release() {
  if (--refCount == 0) {
    delete this;
    return 0;
  }
  return refCount;
}

void PlugView::flashMidiInput() {
  if (webView)
    [webView evaluateJavaScript:@"tram8.flashInput()" completionHandler:nil];
}

void PlugView::flashMidiOutput() {
  if (webView)
    [webView evaluateJavaScript:@"tram8.flashOutput()" completionHandler:nil];
}

} // namespace tram8
