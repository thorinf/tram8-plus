#include "controller.h"
#include "plugview.h"
#include "cids.h"
#include "state_format.h"
#include "base/source/updatehandler.h"
#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"

#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#import <objc/runtime.h>

#include <cassert>
#include <cstdio>
#include <thread>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace tram8;

// Exercise the real controller, parameter notifications and bridge without a
// WebKit process, JS ready message, processor connection or live MIDI output.
@interface TestWebView : NSView
@property(retain) WKWebViewConfiguration* configuration;
@property(retain) NSMutableArray<NSString*>* scripts;
- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration*)configuration;
- (WKNavigation*)loadHTMLString:(NSString*)html baseURL:(NSURL*)url;
- (void)evaluateJavaScript:(NSString*)script completionHandler:(void (^)(id, NSError*))handler;
@end

@implementation TestWebView
- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration*)configuration {
  if ((self = [super initWithFrame:frame])) {
    self.configuration = configuration;
    self.scripts = [NSMutableArray array];
  }
  return self;
}
- (void)dealloc {
  [_configuration release];
  [_scripts release];
  [super dealloc];
}
- (void)setValue:(id)value forKey:(NSString*)key {
  if (![key isEqualToString:@"drawsBackground"])
    [super setValue:value forKey:key];
}
- (WKNavigation*)loadHTMLString:(NSString*)html baseURL:(NSURL*)url {
  return nil;
}
- (void)evaluateJavaScript:(NSString*)script completionHandler:(void (^)(id, NSError*))handler {
  assert([NSThread isMainThread]);
  [self.scripts addObject:script];
  if (handler)
    handler(nil, nil);
}
@end

@interface TestScriptMessage : NSObject
@property(retain) NSDictionary* body;
@end
@implementation TestScriptMessage
- (void)dealloc {
  [_body release];
  [super dealloc];
}
@end

static id lastBridge = nil;
static IMP originalAddHandler = nullptr;

static id allocateWebView(id, SEL) {
  return [TestWebView alloc];
}

static void addHandler(id self, SEL selector, id handler, NSString* name) {
  lastBridge = handler;
  reinterpret_cast<void (*)(id, SEL, id, NSString*)>(originalAddHandler)(self, selector, handler, name);
}

static void drainMainQueue() {
  __block bool done = false;
  dispatch_async(dispatch_get_main_queue(), ^{
    dispatch_async(dispatch_get_main_queue(), ^{
      done = true;
    });
  });
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:5];
  while (!done && [deadline timeIntervalSinceNow] > 0)
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  assert(done);
}

static TestWebView* attach(IPlugView* view, NSView* parent) {
  assert(view->attached(parent, kPlatformTypeNSView) == kResultOk);
  auto* webView = (TestWebView*)parent.subviews.lastObject;
  assert([webView isKindOfClass:[TestWebView class]]);
  [lastBridge setValue:@YES forKey:@"ready"];
  [lastBridge performSelector:@selector(pushState)];
  assert(webView.scripts.count == kNumGates || webView.scripts.count == 1);
  return webView;
}

static NSDictionary* nativeEvent(NSString* script) {
  NSString* prefix = @"typeof tram8 !== 'undefined' && tram8.handleNativeEvent(";
  assert([script hasPrefix:prefix] && [script hasSuffix:@")"]);
  NSString* json = [script substringWithRange:NSMakeRange(prefix.length, script.length - prefix.length - 1)];
  return [NSJSONSerialization JSONObjectWithData:[json dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
}

static void expectGate(TestWebView* view, int gate, int channel, int note, int mode, int dacChannel, int cc) {
  if (view.scripts.count == 1) {
    NSDictionary* state = nativeEvent(view.scripts[0]);
    assert([state[@"type"] isEqual:@"state"] && [state[@"gates"] count] == kNumGates);
    NSDictionary* expected = @{
      @"gate" : @(gate),
      @"channel" : @(channel),
      @"note" : @(note),
      @"mode" : @(mode),
      @"dacChannel" : @(dacChannel),
      @"ccNum" : @(cc)
    };
    assert([state[@"gates"][gate] isEqual:expected]);
    return;
  }
  NSString* expected = [NSString
      stringWithFormat:@"tram8.setGateState(%d, %d, %d, %d, %d, %d)", gate, channel, note, mode, dacChannel, cc];
  assert(view.scripts.count == kNumGates);
  assert([view.scripts[gate] isEqualToString:expected]);
}

int main() {
  @autoreleasepool {
    assert(class_addMethod(
        object_getClass([WKWebView class]), @selector(alloc), reinterpret_cast<IMP>(allocateWebView), "@@:"));
    Method method = class_getInstanceMethod([WKUserContentController class], @selector(addScriptMessageHandler:name:));
    originalAddHandler = method_setImplementation(method, reinterpret_cast<IMP>(addHandler));

    auto* controller = new Controller;
    assert(controller->initialize(nullptr) == kResultOk);
    NSView* parent = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 560, 300)];
    IPlugView* view = controller->createView(ViewType::kEditor);
    auto* webView = [attach(view, parent) retain];
    id bridge = [lastBridge retain];
    for (int i = 0; i < kNumGates; i++)
      expectGate(webView, i, -1, 60 + i, kDacVelocity, -1, 1);

    [webView.scripts removeAllObjects];
    for (int i = 0; i < kNumGates; i++) {
      assert(controller->setParamNormalized(kGateChannelBase + i, (i + 1) / 16.0) == kResultOk);
      assert(controller->setParamNormalized(kGateNoteBase + i, (81 + i) / 128.0) == kResultOk);
      assert(controller->setParamNormalized(kDacModeBase + i, ((i + 1) % kDacModeCount) / 3.0) == kResultOk);
      assert(controller->setParamNormalized(kDacChannelBase + i, (16 - i) / 16.0) == kResultOk);
      assert(controller->setParamNormalized(kCcNumBase + i, (100 + i) / 127.0) == kResultOk);
    }
    assert(webView.scripts.count == 0);
    drainMainQueue();
    for (int i = 0; i < kNumGates; i++)
      expectGate(webView, i, i, 80 + i, (i + 1) % kDacModeCount, 15 - i, 100 + i);

    [webView.scripts removeAllObjects];
    assert(controller->setParamNormalized(kCcValueBase, 1.0) == kResultOk);
    assert(controller->setParamNormalized(kGateChannelBase, 1 / 16.0) == kResultOk);
    drainMainQueue();
    assert(webView.scripts.count == 0); // hidden and unchanged parameters do not refresh settings

    controller->getParameterObject(kGateNoteBase)->setNormalized(0);
    assert(controller->setParamNormalized(kGateChannelBase, 2.0) == kResultOk);
    drainMainQueue();
    expectGate(webView, 0, 15, -1, kDacPitch, 15, 100);

    [webView.scripts removeAllObjects];
    int32_t words[kNumGates * MidiEngine::kStateWordsPerGate];
    for (int i = 0; i < kNumGates; i++) {
      int off = i * MidiEngine::kStateWordsPerGate;
      words[off] = i == 0 ? -1 : 15;
      words[off + 1] = i == 0 ? -1 : 127;
      words[off + 2] = i % kDacModeCount;
      words[off + 3] = i == 0 ? -1 : 0;
      words[off + 4] = i == 0 ? 0 : 127;
    }
    MemoryStream state;
    assert(writeStateWords(&state, words));
    assert(state.seek(0, IBStream::kIBSeekSet, nullptr) == kResultOk);
    assert(controller->setComponentState(&state) == kResultOk);
    drainMainQueue();
    for (int i = 0; i < kNumGates; i++)
      expectGate(webView, i, words[i * 5], words[i * 5 + 1], words[i * 5 + 2], words[i * 5 + 3], words[i * 5 + 4]);

    [webView.scripts removeAllObjects];
    std::thread worker([&] { static_cast<PlugView*>(view)->update(nullptr, IDependent::kChanged); });
    worker.join();
    drainMainQueue();
    expectGate(webView, 0, -1, -1, kDacVelocity, -1, 0); // evaluateJavaScript asserts main-thread delivery

    [webView.scripts removeAllObjects];
    assert(controller->setParamNormalized(kGateNoteBase, 61 / 128.0) == kResultOk);
    assert(view->removed() == kResultOk);
    drainMainQueue();
    assert(webView.scripts.count == 0); // pending settings cannot reach a detached view
    assert(controller->setParamNormalized(kGateNoteBase, 62 / 128.0) == kResultOk);
    drainMainQueue();
    assert(webView.scripts.count == 0);
    TestScriptMessage* lateMessage = [[[TestScriptMessage alloc] init] autorelease];
    lateMessage.body = @{@"type" : @"setNote", @"gate" : @0, @"note" : @0};
    [bridge userContentController:webView.configuration.userContentController
          didReceiveScriptMessage:(WKScriptMessage*)lateMessage];
    assert(controller->getParamNormalized(kGateNoteBase) == 62 / 128.0);

    TestWebView* reopened = attach(view, parent);
    expectGate(reopened, 0, -1, 61, kDacVelocity, -1, 0);
    [reopened.scripts removeAllObjects];
    assert(controller->setParamNormalized(kGateNoteBase, 63 / 128.0) == kResultOk);
    drainMainQueue();
    expectGate(reopened, 0, -1, 62, kDacVelocity, -1, 0);

    // Releasing an older view must not clear the current editor's activity target.
    IPlugView* replacement = controller->createView(ViewType::kEditor);
    TestWebView* replacementWebView = [attach(replacement, parent) retain];
    [replacementWebView.scripts removeAllObjects];
    view->release(); // host omitted removed(); destructor must clean up
    HostMessage activity;
    activity.setMessageID("MidiActivity");
    assert(activity.getAttributes()->setInt("input", 1) == kResultOk);
    assert(controller->notify(&activity) == kResultOk);
    drainMainQueue();
    assert(replacementWebView.scripts.count == 1);
    NSString* activityScript = replacementWebView.scripts[0];
    if (![activityScript isEqualToString:@"tram8.flashInput()"])
      assert(([nativeEvent(activityScript) isEqual:@{@"type" : @"activity", @"input" : @YES}]));

    [replacementWebView.scripts removeAllObjects];
    assert(controller->setParamNormalized(kGateNoteBase, 64 / 128.0) == kResultOk);
    replacement->release(); // close while a settings refresh is queued
    drainMainQueue();
    assert(controller->setParamNormalized(kGateNoteBase, 65 / 128.0) == kResultOk);
    assert(controller->notify(&activity) == kResultOk);
    drainMainQueue();
    assert(replacementWebView.scripts.count == 0);

    IPlugView* terminatingView = controller->createView(ViewType::kEditor);
    TestWebView* terminatingWebView = [attach(terminatingView, parent) retain];
    [terminatingWebView.scripts removeAllObjects];
    IPtr<Parameter> observedNote(controller->getParameterObject(kGateNoteBase));
    auto* updates = UpdateHandler::instance();
    assert(updates->countDependencies(observedNote.get()) == 1);
    assert(controller->setParamNormalized(kGateNoteBase, 66 / 128.0) == kResultOk);
    assert(controller->terminate() == kResultOk);
    assert(controller->getParameterCount() == 0);
    controller->release(); // view holds the remaining controller reference
    terminatingView->release(); // refresh is still queued after the parameter list was cleared
    assert(updates->countDependencies(observedNote.get()) == 0);
    assert(updates->countDependencies() == 0);
    drainMainQueue();
    assert(terminatingWebView.scripts.count == 0);
    assert(observedNote->setNormalized(67 / 128.0));
    drainMainQueue();
    assert(terminatingWebView.scripts.count == 0);
    [terminatingWebView release];

    [replacementWebView release];
    [bridge release];
    [webView release];
    [parent release];
    puts("editor_sync passed (host parameters, component state, main queue, detach/reopen/close, terminate before "
         "release)");
  }
}
