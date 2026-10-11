// The Taptic Engine for Haptics.h: 0 a light tap, 1 a firm one, 2 the
// system's "error" pattern (a missed drill answer). The generators are
// made once and kept prepared -- one created and fired in the same
// instant can miss its first tap while the engine spins up.
#import <UIKit/UIKit.h>

static UIImpactFeedbackGenerator* lightTap;
static UIImpactFeedbackGenerator* firmTap;
static UINotificationFeedbackGenerator* notice;

void UB_Haptic(int kind)
{
    @autoreleasepool {
        if (!lightTap) {
            lightTap = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleMedium];
            firmTap = [[UIImpactFeedbackGenerator alloc] initWithStyle:UIImpactFeedbackStyleHeavy];
            notice = [[UINotificationFeedbackGenerator alloc] init];
        }
        if (kind == 2) {
            [notice notificationOccurred:UINotificationFeedbackTypeError];
            [notice prepare];
        } else if (kind == 1) {
            [firmTap impactOccurred];
            [firmTap prepare];
        } else {
            [lightTap impactOccurred];
            [lightTap prepare];
        }
    }
}
