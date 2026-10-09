// Turns the iPhone/iPad to portrait for the training drills and back to
// landscape for everything else (see updateOrientation() in mina.cpp).
// mina.cpp first sets SDL_HINT_ORIENTATIONS, which is what SDL's view
// controller reports as its supported orientations; this then asks iOS to
// re-read that and rotate.
#import <UIKit/UIKit.h>
#include <SDL3/SDL.h>

void UB_RequestOrientation(SDL_Window* window, bool portrait)
{
    @autoreleasepool {
        UIWindow* uiwindow = (__bridge UIWindow*)SDL_GetPointerProperty(SDL_GetWindowProperties(window),
            SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, NULL);
        if (!uiwindow) {
            return;
        }
        UIViewController* controller = uiwindow.rootViewController;
        if (@available(iOS 16.0, *)) {
            [controller setNeedsUpdateOfSupportedInterfaceOrientations];
            UIWindowScene* scene = uiwindow.windowScene;
            UIInterfaceOrientationMask mask = portrait ? UIInterfaceOrientationMaskPortrait
                                                       : UIInterfaceOrientationMaskLandscape;
            UIWindowSceneGeometryPreferencesIOS* preferences =
                [[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
            [scene requestGeometryUpdateWithPreferences:preferences errorHandler:nil];
#if !__has_feature(objc_arc)
            [preferences release];
#endif
        } else {
            [UIViewController attemptRotationToDeviceOrientation];
        }
    }
}
