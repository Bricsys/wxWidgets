#include "testprec.h"

#import "wx/osx/cocoa/trackerTouchDouble.h"

@interface wxTestTouch : NSObject
@property(readonly) NSNumber *identity;
@property(readonly) NSPoint normalizedPosition;
@property(readonly) NSSize deviceSize;
+ (NSUInteger)liveCount;
- (id)initWithIdentity:(NSUInteger)identity position:(NSPoint)position;
@end

static NSUInteger gs_liveTouches = 0;

@implementation wxTestTouch
+ (NSUInteger)liveCount { return gs_liveTouches; }
- (id)initWithIdentity:(NSUInteger)identity position:(NSPoint)position
{
    if ((self = [super init]))
    {
        _identity = [[NSNumber alloc] initWithUnsignedInteger:identity];
        _normalizedPosition = position;
        _deviceSize = NSMakeSize(100, 100);
        ++gs_liveTouches;
    }
    return self;
}
- (void)dealloc
{
    [_identity release];
    --gs_liveTouches;
    [super dealloc];
}
@end

@interface wxTestTouchSet : NSSet
{
    NSArray *_touches;
}
- (id)initWithTouches:(NSArray *)touches;
@end

@implementation wxTestTouchSet
- (id)initWithTouches:(NSArray *)touches
{
    if ((self = [super init]))
        _touches = [touches copy];
    return self;
}
- (NSUInteger)count { return _touches.count; }
- (NSArray *)allObjects { return _touches; }
- (NSEnumerator *)objectEnumerator { return [_touches objectEnumerator]; }
- (id)member:(id)object
{
    NSUInteger index = [_touches indexOfObject:object];
    return index == NSNotFound ? nil : [_touches objectAtIndex:index];
}
- (void)dealloc
{
    [_touches release];
    [super dealloc];
}
@end

@interface wxTestTouchEvent : NSEvent
{
    NSSet *_touches;
}
- (id)initWithIdentities:(NSArray *)identities offset:(CGFloat)offset;
@end

@implementation wxTestTouchEvent
- (id)initWithIdentities:(NSArray *)identities offset:(CGFloat)offset
{
    if ((self = [super init]))
    {
        NSMutableArray *touches = [[NSMutableArray alloc] init];
        for (NSNumber *identity in identities)
        {
            wxTestTouch *touch = [[wxTestTouch alloc]
                initWithIdentity:identity.unsignedIntegerValue
                position:NSMakePoint(identity.unsignedIntegerValue * 0.1 + offset, 0.5)];
            [touches addObject:touch];
            [touch release];
        }
        _touches = [[wxTestTouchSet alloc] initWithTouches:touches];
        [touches release];
    }
    return self;
}
- (NSSet *)touchesMatchingPhase:(NSTouchPhase)phase inView:(NSView *)view
{
    wxUnusedVar(phase);
    wxUnusedVar(view);
    return _touches;
}
- (NSEventModifierFlags)modifierFlags { return 0; }
- (NSPoint)locationInWindow { return NSZeroPoint; }
- (void)dealloc
{
    [_touches release];
    [super dealloc];
}
@end

@interface wxTestTouchView : NSView
@property NSUInteger begins;
@property NSUInteger updates;
@property NSUInteger ends;
- (void)beginTouch:(DualTouchTracker *)tracker;
- (void)updateTouch:(DualTouchTracker *)tracker;
- (void)endTouch:(DualTouchTracker *)tracker;
@end

@implementation wxTestTouchView
- (void)beginTouch:(DualTouchTracker *)tracker
{
    wxUnusedVar(tracker);
    ++_begins;
}
- (void)updateTouch:(DualTouchTracker *)tracker
{
    wxUnusedVar(tracker);
    ++_updates;
}
- (void)endTouch:(DualTouchTracker *)tracker
{
    CHECK([tracker getCurrentTouchEvent] != nil);
    ++_ends;
}
@end

namespace
{
struct TouchTrackerFixture
{
    TouchTrackerFixture()
    {
        pool = [[NSAutoreleasePool alloc] init];
        [NSApplication sharedApplication];
        view = [[wxTestTouchView alloc] init];
        tracker = [[DualTouchTracker alloc] init];
        tracker.view = view;
        tracker.beginTrackingAction = @selector(beginTouch:);
        tracker.updateTrackingAction = @selector(updateTouch:);
        tracker.endTrackingAction = @selector(endTouch:);
    }

    ~TouchTrackerFixture()
    {
        [tracker release];
        [view release];
        [pool drain];
    }

    void Send(SEL selector, NSArray *identities, CGFloat offset = 0)
    {
        // Drain event-owned touches between deliveries, as Cocoa does.
        @autoreleasepool
        {
            wxTestTouchEvent *event = [[wxTestTouchEvent alloc]
                initWithIdentities:identities offset:offset];
            [tracker performSelector:selector withObject:event];
            [event release];
        }
    }

    NSAutoreleasePool *pool;
    wxTestTouchView *view;
    DualTouchTracker *tracker;
};
}

TEST_CASE("DualTouchTracker keeps a stable pair", "[touchtracker]")
{
    TouchTrackerFixture f;
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @2]);
    f.Send(@selector(touchesMovedWithEvent:), @[@2, @1], 0.05);
    CHECK(f.view.begins == 1);
    CHECK(f.tracker.deltaOrigin.x == Approx(5));
    f.Send(@selector(touchesMovedWithEvent:), @[@1, @2], 0.1);
    CHECK(f.view.updates == 1);
    CHECK(f.tracker.deltaOrigin.x == Approx(10));
    f.Send(@selector(touchesEndedWithEvent:), @[]);
    CHECK(f.view.ends == 1);
    CHECK([f.tracker getCurrentTouchEvent] == nil);
    CHECK([wxTestTouch liveCount] == 0);
}

TEST_CASE("DualTouchTracker cancels a replaced finger", "[touchtracker]")
{
    TouchTrackerFixture f;
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @2]);
    f.Send(@selector(touchesMovedWithEvent:), @[@1, @2], 0.05);
    f.Send(@selector(touchesMovedWithEvent:), @[@1, @2], 0.075);

    SECTION("First finger replaced")
    {
        f.Send(@selector(touchesMovedWithEvent:), @[@2, @3], 0.1);
    }
    SECTION("Second finger replaced")
    {
        f.Send(@selector(touchesMovedWithEvent:), @[@1, @3], 0.1);
    }
    SECTION("Both fingers replaced")
    {
        f.Send(@selector(touchesMovedWithEvent:), @[@3, @4], 0.1);
    }

    f.Send(@selector(touchesMovedWithEvent:), @[@2, @3], 0.15);
    CHECK(f.view.ends == 1);
    REQUIRE([f.tracker getCurrentTouchEvent] == nil);
    CHECK([wxTestTouch liveCount] == 0);
    CHECK(f.view.updates == 1);
}

TEST_CASE("DualTouchTracker clears incomplete gestures", "[touchtracker]")
{
    TouchTrackerFixture f;
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @2]);

    SECTION("Lift one finger before crossing the threshold")
    {
        f.Send(@selector(touchesEndedWithEvent:), @[@1]);
    }
    SECTION("Cancel before crossing the threshold")
    {
        f.Send(@selector(touchesCancelledWithEvent:), @[]);
    }
    SECTION("Disable before crossing the threshold")
    {
        f.tracker.isEnabled = NO;
    }
    SECTION("Add a third finger before crossing the threshold")
    {
        f.Send(@selector(touchesBeganWithEvent:), @[@1, @2, @3]);
    }

    CHECK([f.tracker getCurrentTouchEvent] == nil);
    CHECK(f.tracker.deltaOrigin.x == 0);
    CHECK(f.view.ends == 0);
    CHECK([wxTestTouch liveCount] == 0);
}

TEST_CASE("DualTouchTracker restarts after lifting one finger", "[touchtracker]")
{
    TouchTrackerFixture f;
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @2]);
    f.Send(@selector(touchesMovedWithEvent:), @[@1, @2], 0.05);
    f.Send(@selector(touchesEndedWithEvent:), @[@1]);
    CHECK(f.view.ends == 1);
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @3]);
    f.Send(@selector(touchesMovedWithEvent:), @[@3, @1], 0.1);
    CHECK(f.view.begins == 2);
    CHECK(f.tracker.deltaOrigin.x == Approx(10));
    f.Send(@selector(touchesCancelledWithEvent:), @[]);
    [f.tracker cancelTracking];
    CHECK(f.view.ends == 2);
    CHECK([wxTestTouch liveCount] == 0);
}

TEST_CASE("DualTouchTracker replaces an unfinished pair without leaking", "[touchtracker]")
{
    TouchTrackerFixture f;
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @2]);
    f.Send(@selector(touchesBeganWithEvent:), @[@1, @3]);
    CHECK([wxTestTouch liveCount] == 2);
    [f.tracker cancelTracking];
    CHECK([wxTestTouch liveCount] == 0);
    CHECK(f.view.ends == 0);
}
