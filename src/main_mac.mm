#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

#include "app_state.h"
#include "bridge_js.h"

#include <memory>

// WebView2's chrome.webview API on top of WKWebView, so ui/app.js runs unchanged.
static NSString* const kWebviewShim =
    @"window.chrome = window.chrome || {};"
     "window.chrome.webview = {"
     "  _l: [],"
     "  postMessage(s) { window.webkit.messageHandlers.mstn.postMessage(s); },"
     "  addEventListener(t, f) { if (t === 'message') this._l.push(f); },"
     "  _recv(d) { this._l.forEach((f) => f({ data: d })); }"
     "};";

@interface AppDelegate : NSObject <NSApplicationDelegate, WKScriptMessageHandler>
@end

@implementation AppDelegate {
  std::unique_ptr<AppState> app_;
  NSWindow* window_;
  WKWebView* web_;
  NSTimer* timer_;
}

- (void)applicationDidFinishLaunching:(NSNotification*)note {
  app_ = std::make_unique<AppState>();

  WKWebViewConfiguration* cfg = [WKWebViewConfiguration new];
  [cfg.userContentController
      addUserScript:[[WKUserScript alloc] initWithSource:kWebviewShim
                                           injectionTime:WKUserScriptInjectionTimeAtDocumentStart
                                        forMainFrameOnly:YES]];
  [cfg.userContentController addScriptMessageHandler:self name:@"mstn"];

  // Locked ~16:10 client, same as the Windows build.
  const NSRect frame = NSMakeRect(0, 0, 1280, 820);
  window_ = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  window_.title = @"mstnCon — Type2";
  window_.releasedWhenClosed = NO;
  web_ = [[WKWebView alloc] initWithFrame:frame configuration:cfg];
  web_.pageZoom = 1.06;
  if (@available(macOS 13.3, *)) web_.inspectable = YES;
  window_.contentView = web_;
  [window_ center];
  [window_ makeKeyAndOrderFront:nil];

  NSURL* ui = [NSBundle.mainBundle.resourceURL URLByAppendingPathComponent:@"ui"];
  [web_ loadFileURL:[ui URLByAppendingPathComponent:@"index.html"] allowingReadAccessToURL:ui];

  WKWebView* web = web_;
  app_->set_ui_push([web](const std::string& json) {
    if (NSString* s = [NSString stringWithUTF8String:json.c_str()])
      [web evaluateJavaScript:[NSString stringWithFormat:@"chrome.webview._recv(%@)", s] completionHandler:nil];
  });

  // ~30 Hz; common modes keep it ticking while a menu or the window drag is tracking.
  AppState* app = app_.get();
  timer_ = [NSTimer timerWithTimeInterval:0.033 repeats:YES block:^(NSTimer*) { app->ui_tick(); }];
  [NSRunLoop.mainRunLoop addTimer:timer_ forMode:NSRunLoopCommonModes];
  app_->start_usb();

  if (!CGPreflightPostEventAccess()) {
    CGRequestPostEventAccess();
    app_->push_toast("Accessibility",
                     "อนุญาต mstnCon ใน System Settings → Privacy & Security → Accessibility เพื่อส่งปุ่มเข้าเกม");
  }
}

- (void)userContentController:(WKUserContentController*)controller
      didReceiveScriptMessage:(WKScriptMessage*)message {
  if ([message.body isKindOfClass:NSString.class]) handle_ui_command(*app_, [message.body UTF8String]);
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender {
  return YES;
}

- (void)applicationWillTerminate:(NSNotification*)note {
  [timer_ invalidate];
  app_.reset(); // stops USB (lamps/vib off) and releases any held keys
}
@end

int main() {
  @autoreleasepool {
    NSApplication* app = NSApplication.sharedApplication;
    app.activationPolicy = NSApplicationActivationPolicyRegular;

    NSMenu* bar = [NSMenu new];
    NSMenu* appMenu = [NSMenu new];
    [appMenu addItemWithTitle:@"Quit mstnCon" action:@selector(terminate:) keyEquivalent:@"q"];
    [bar addItemWithTitle:@"" action:nil keyEquivalent:@""].submenu = appMenu;
    app.mainMenu = bar;

    AppDelegate* delegate = [AppDelegate new];
    app.delegate = delegate;
    [app run];
  }
  return 0;
}
