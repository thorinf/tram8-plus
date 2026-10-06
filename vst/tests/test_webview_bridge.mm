#include "controller.h"
#include "plugview.h"
#include "cids.h"
#include "midi_engine.h"
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

@interface TestWebView : NSView
@property(retain) WKWebViewConfiguration* configuration;
@property(retain) NSMutableArray* scripts;
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
  assert([NSThread isMainThread]);
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
}
@end

@interface TestScriptMessage : NSObject
@property(retain) id body;
@end
@implementation TestScriptMessage
- (void)dealloc {
  [_body release];
  [super dealloc];
}
@end

static id lastBridge;
static IMP originalAddHandler;
static id allocateWebView(id, SEL) {
  return [TestWebView alloc];
}
static void addHandler(id self, SEL selector, id handler, NSString* name) {
  lastBridge = handler;
  reinterpret_cast<void (*)(id, SEL, id, NSString*)>(originalAddHandler)(self, selector, handler, name);
}
static void send(id bridge, id body) {
  TestScriptMessage* message = [[[TestScriptMessage alloc] init] autorelease];
  message.body = body;
  WKUserContentController* content = [[[WKUserContentController alloc] init] autorelease];
  [bridge userContentController:content didReceiveScriptMessage:(WKScriptMessage*)message];
}
static void drain() {
  __block bool done = false;
  dispatch_async(dispatch_get_main_queue(), ^{
    done = true;
  });
  NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:5];
  while (!done && deadline.timeIntervalSinceNow > 0)
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
  assert(done);
}
static NSDictionary* event(NSString* script) {
  NSString* prefix = @"typeof tram8 !== 'undefined' && tram8.handleNativeEvent(";
  assert([script hasPrefix:prefix] && [script hasSuffix:@")"]);
  NSString* json = [script substringWithRange:NSMakeRange(prefix.length, script.length - prefix.length - 1)];
  return [NSJSONSerialization JSONObjectWithData:[json dataUsingEncoding:NSUTF8StringEncoding] options:0 error:nil];
}

int main() {
  @autoreleasepool {
    assert(class_addMethod(object_getClass([WKWebView class]), @selector(alloc), (IMP)allocateWebView, "@@:"));
    Method method = class_getInstanceMethod([WKUserContentController class], @selector(addScriptMessageHandler:name:));
    originalAddHandler = method_setImplementation(method, (IMP)addHandler);
    auto* controller = new Controller;
    assert(controller->initialize(nullptr) == kResultOk);
    NSView* parent = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 560, 300)];
    IPlugView* view = controller->createView(ViewType::kEditor);
    assert(view->attached(nullptr, kPlatformTypeNSView) == kInvalidArgument);
    assert(view->attached(parent, kPlatformTypeNSView) == kResultOk);
    id bridge = [lastBridge retain];
    TestWebView* webView = [(TestWebView*)parent.subviews.lastObject retain];
    [bridge performSelector:@selector(pushState)];
    NSDictionary* state = event(webView.scripts.lastObject);
    assert([state[@"type"] isEqual:@"state"] && [state[@"gates"] count] == kNumGates);
    assert([state[@"gates"][0][@"note"] intValue] == 60);
    send(bridge, @{@"type" : @"setNote", @"gate" : @0, @"note" : @127});
    assert(controller->getParamNormalized(kGateNoteBase) == 1.0);

    for (id bad in @[ [NSNull null], @[], @{}, @"0", @YES, @0.5, @(-2), @128, @(NAN), @(INFINITY) ]) {
      send(bridge, @{@"type" : @"setNote", @"gate" : @0, @"note" : bad});
      send(bridge, @{@"type" : @"setNote", @"gate" : bad, @"note" : @0});
    }
    send(bridge, @{@"type" : @"setNote", @"gate" : @8, @"note" : @0});
    send(bridge, @{@"type" : @"setNote", @"gate" : @0});
    send(bridge, @{@"type" : [NSNull null]});
    send(bridge, @[]);
    assert(controller->getParamNormalized(kGateNoteBase) == 1.0);
    for (NSDictionary* bad in @[
           @{@"type" : @"setChannel",
             @"gate" : @0,
             @"channel" : @16},
           @{@"type" : @"setDacChannel",
             @"gate" : @0,
             @"channel" : @(-2)},
           @{@"type" : @"setDacMode",
             @"gate" : @0,
             @"mode" : @4},
           @{@"type" : @"setCcNum",
             @"gate" : @0,
             @"cc" : @128},
           @{@"type" : @"setMidiPort", @"index" : [NSNull null]},
           @{@"type" : @"resize", @"height" : @"300"}
         ])
      send(bridge, bad);
    assert(controller->getParamNormalized(kGateChannelBase) == 0);
    assert(controller->getParamNormalized(kDacChannelBase) == 0);
    assert(controller->getParamNormalized(kDacModeBase) == 0);
    assert(controller->getParamNormalized(kCcNumBase) == 1 / 127.0);

    HostMessage activity;
    activity.setMessageID("MidiActivity");
    activity.getAttributes()->setInt("input", 1);
    [webView.scripts removeAllObjects];
    assert(controller->notify(&activity) == kResultOk);
    assert(view->removed() == kResultOk);
    send(bridge, @{@"type" : @"setNote", @"gate" : @0, @"note" : @0});
    drain();
    assert(webView.scripts.count == 0);
    assert(controller->getParamNormalized(kGateNoteBase) == 1.0);
    assert(view->attached(parent, kPlatformTypeNSView) == kResultOk);
    TestWebView* reopened = (TestWebView*)parent.subviews.lastObject;
    controller->notify(&activity);
    drain();
    assert([event(reopened.scripts.lastObject)[@"input"] boolValue]);

    IPlugView* replacement = controller->createView(ViewType::kEditor);
    assert(replacement->attached(parent, kPlatformTypeNSView) == kResultOk);
    TestWebView* replacementWebView = [(TestWebView*)parent.subviews.lastObject retain];
    view->release();
    controller->notify(&activity);
    drain();
    assert(replacementWebView.scripts.count == 1);
    [replacementWebView.scripts removeAllObjects];
    std::thread worker([&] {
      for (int i = 0; i < 100; ++i)
        controller->notify(&activity);
    });
    replacement->release(); // host omitted removed(), racing activity notification
    worker.join();
    drain();
    assert(controller->notify(&activity) == kResultOk);
    drain();

    IPlugView* surviving = controller->createView(ViewType::kEditor);
    assert(controller->terminate() == kResultOk);
    controller->release(); // un-attached views also keep their controller alive
    std::thread releaseWorker([&] { surviving->release(); });
    releaseWorker.join();
    drain();
    [replacementWebView release];
    [webView release];
    [bridge release];
    [parent release];
    puts("webview bridge passed (structured events, validation, detach/reopen, concurrent close)");
  }
}
