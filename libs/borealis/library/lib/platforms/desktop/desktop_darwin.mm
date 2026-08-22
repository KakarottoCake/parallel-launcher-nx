/*
    Copyright 2021 natinusala
    Copyright 2023 xfangfang

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <string>
#include <functional>
#include <chrono>

#include <AvailabilityMacros.h>

#if defined(BOREALIS_USE_METAL)
    #include <borealis/core/logger.hpp>
    #import <AppKit/AppKit.h>
    #import <QuartzCore/CADisplayLink.h>
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED >= 1060
    #define HAS_CORE_WLAN 1
#else
    #define HAS_CORE_WLAN 0
#endif

#if HAS_CORE_WLAN
    #import <CoreWLAN/CoreWLAN.h>
#endif

#if defined(BOREALIS_USE_METAL)

API_AVAILABLE(macos(14.0))
@interface BRLSDarwinDisplayLinkTarget : NSObject
{
  @public
    std::function<bool()> runLoopImpl;
    std::chrono::steady_clock::time_point measurementStart;
    size_t frameCount;
    BOOL measurementLogged;
}
- (void)displayLinkDidFire:(CADisplayLink*)sender;
@end

@implementation BRLSDarwinDisplayLinkTarget
- (instancetype)init
{
    self = [super init];
    if (self != nil)
    {
        measurementStart = std::chrono::steady_clock::now();
        frameCount = 0;
        measurementLogged = NO;
    }
    return self;
}

- (void)displayLinkDidFire:(CADisplayLink*)sender
{
    @autoreleasepool
    {
        frameCount++;
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - measurementStart;
        if (!measurementLogged && elapsed >= std::chrono::seconds(2))
        {
            double seconds = std::chrono::duration<double>(elapsed).count();
            brls::Logger::info("darwin: measured Metal UI loop at {:.2f} FPS",
                static_cast<double>(frameCount) / seconds);
            measurementLogged = YES;
        }

        if (!runLoopImpl())
        {
            [sender invalidate];
            [NSApp stop:nil];
        }
    }
}
@end

#endif

namespace brls
{

#if defined(BOREALIS_USE_METAL)

namespace
{

static NSWindow* applicationWindow()
{
    NSWindow* window = NSApp.mainWindow;
    if (window == nil)
        window = NSApp.keyWindow;
    if (window == nil)
    {
        for (NSWindow* candidate in NSApp.windows)
        {
            if (candidate.visible)
            {
                window = candidate;
                break;
            }
        }
    }

    return window;
}

} // namespace

#endif

// Interface method, fetching the current connection info.
int darwin_wlan_quality() {
#if HAS_CORE_WLAN
#ifdef __clang__
    @autoreleasepool {
        CWWiFiClient* Client = CWWiFiClient.sharedWiFiClient;
        CWInterface* currentInterface = Client.interface;
        if ([currentInterface powerOn] == false) {
            return -1;
        }
        if ([currentInterface serviceActive] == false) {
            return 0;
        }
        int rssi = [currentInterface rssiValue];
        if (rssi > -50) return 3;
        if (rssi > -80) return 2;
        return 1;
    }
#else
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    CWInterface* currentInterface = [CWInterface interface];
    if (![currentInterface ssid]) {
        [pool drain];
        return 0;
    }
    NSNumber* rssiNumber = [currentInterface rssi];
    int result = 1;
    if (rssiNumber) {
        int rssi = [rssiNumber intValue];
        if (rssi > -50) result = 3;
        else if (rssi > -80) result = 2;
    }
    [pool drain];
    return result;
#endif
#else // HAS_CORE_WLAN
    return -1;
#endif
}

bool darwin_runloop(const std::function<bool()>& runLoopImpl) {
#ifdef __clang__
    @autoreleasepool {
#if defined(BOREALIS_USE_METAL)
        if (@available(macOS 14.0, *))
        {
            NSWindow* window = applicationWindow();
            if (window != nil)
            {
                // Mirror SDL3's iOS main-callback path: CADisplayLink owns the
                // run loop and invokes one complete application iteration for
                // each requested display update.
                auto* target = [[BRLSDarwinDisplayLinkTarget alloc] init];
                target->runLoopImpl = runLoopImpl;

                CADisplayLink* displayLink = [window
                    displayLinkWithTarget:target
                    selector:@selector(displayLinkDidFire:)];
                if (displayLink != nil)
                {
                    NSInteger frameRate = window.screen.maximumFramesPerSecond;
                    if (frameRate <= 0)
                        frameRate = 60;
                    displayLink.preferredFrameRateRange = CAFrameRateRangeMake(
                        static_cast<float>((frameRate * 2) / 3),
                        static_cast<float>(frameRate),
                        static_cast<float>(frameRate));
                    [displayLink addToRunLoop:NSRunLoop.currentRunLoop
                        forMode:NSDefaultRunLoopMode];

                    Logger::info("darwin: Metal UI requested {} Hz CADisplayLink",
                        frameRate);
                    [NSApp run];
                    [displayLink invalidate];
#if !__has_feature(objc_arc)
                    [target release];
#endif
                    return false;
                }

#if !__has_feature(objc_arc)
                [target release];
#endif
                Logger::error("darwin: failed to create Metal CADisplayLink");
            }
        }
#endif
        return runLoopImpl();
    }
#else
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    bool result = runLoopImpl();
    [pool drain];
    return result;
#endif
}

std::string darwin_locale() {
#ifdef __clang__
    @autoreleasepool {
        NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
        NSArray *languages = [defaults objectForKey:@"AppleLanguages"];
        NSString *current = [languages objectAtIndex:0];
        return [current UTF8String];
    }
#else
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *languages = [defaults objectForKey:@"AppleLanguages"];
    NSString *current = [languages objectAtIndex:0];
    [pool drain];
    return [current UTF8String];
#endif
}

}
