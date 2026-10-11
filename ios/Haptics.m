// The Taptic Engine for Haptics.h: 0 a light tap, 1 a firm one, 2 the
// system's "error" pattern (a missed drill answer).
#import <UIKit/UIKit.h>

void UB_Haptic(int kind)
{
    @autoreleasepool {
        if (kind == 2) {
            UINotificationFeedbackGenerator* g = [[UINotificationFeedbackGenerator alloc] init];
            [g notificationOccurred:UINotificationFeedbackTypeError];
#if !__has_feature(objc_arc)
            [g release];
#endif
            return;
        }
        UIImpactFeedbackGenerator* g = [[UIImpactFeedbackGenerator alloc]
            initWithStyle:(kind == 0 ? UIImpactFeedbackStyleLight : UIImpactFeedbackStyleHeavy)];
        [g impactOccurred];
#if !__has_feature(objc_arc)
        [g release];
#endif
    }
}
