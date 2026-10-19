// BACKUP on iPhone/iPad (see Backup.h): SDL has no file dialog on iOS, so
// saving offers the backup file through the share sheet (Save to Files,
// AirDrop, Mail...) and loading opens the Files picker. The picker answers
// later; Backup::poll() collects the answer with UB_TakePickedFile().
#import <UIKit/UIKit.h>
#include <string.h>

static UIViewController* UB_TopController(void)
{
    UIWindow* window = nil;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) {
            continue;
        }
        for (UIWindow* w in ((UIWindowScene*)scene).windows) {
            if (w.isKeyWindow) {
                window = w;
                break;
            }
        }
    }
    UIViewController* controller = window.rootViewController;
    while (controller.presentedViewController) {
        controller = controller.presentedViewController;
    }
    return controller;
}

// On iPad a share sheet or picker has to hang off something on screen.
static void UB_Anchor(UIViewController* sheet, UIViewController* host)
{
    UIPopoverPresentationController* popover = sheet.popoverPresentationController;
    if (popover) {
        popover.sourceView = host.view;
        popover.sourceRect = CGRectMake(CGRectGetMidX(host.view.bounds), CGRectGetMidY(host.view.bounds), 1, 1);
        popover.permittedArrowDirections = 0;
    }
}

void UB_ShareFile(const char* path)
{
    @autoreleasepool {
        UIViewController* host = UB_TopController();
        if (!host) {
            return;
        }
        NSURL* url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
        UIActivityViewController* sheet = [[UIActivityViewController alloc] initWithActivityItems:@[ url ] applicationActivities:nil];
        UB_Anchor(sheet, host);
        [host presentViewController:sheet animated:YES completion:nil];
#if !__has_feature(objc_arc)
        [sheet release];
#endif
    }
}

// What the picker chose: 0 nothing yet, 1 a file (UB_pickedPath), -1 cancelled.
static int UB_pickState = 0;
static char UB_pickedPath[2048];

@interface UBBackupPickerDelegate : NSObject <UIDocumentPickerDelegate>
@end

@implementation UBBackupPickerDelegate
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    NSURL* url = urls.firstObject;
    if (url && url.path) {
        strncpy(UB_pickedPath, url.path.UTF8String, sizeof(UB_pickedPath) - 1);
        UB_pickedPath[sizeof(UB_pickedPath) - 1] = 0;
        UB_pickState = 1;
    } else {
        UB_pickState = -1;
    }
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
    UB_pickState = -1;
}
@end

static UBBackupPickerDelegate* UB_pickerDelegate = nil;

void UB_PickBackupFile(void)
{
    @autoreleasepool {
        UB_pickState = 0;
        UIViewController* host = UB_TopController();
        if (!host) {
            UB_pickState = -1;
            return;
        }
        if (!UB_pickerDelegate) {
            UB_pickerDelegate = [[UBBackupPickerDelegate alloc] init]; // kept for the app's life
        }
        // Import mode copies the chosen file into the app, so it can be read
        // without asking for access to the place it came from.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc]
            initWithDocumentTypes:@[ @"public.plain-text", @"public.text", @"public.data" ]
                           inMode:UIDocumentPickerModeImport];
#pragma clang diagnostic pop
        picker.delegate = UB_pickerDelegate;
        UB_Anchor(picker, host);
        [host presentViewController:picker animated:YES completion:nil];
#if !__has_feature(objc_arc)
        [picker release];
#endif
    }
}

int UB_TakePickedFile(char* buf, int size)
{
    int state = UB_pickState;
    if (state == 1 && size > 0) {
        strncpy(buf, UB_pickedPath, size - 1);
        buf[size - 1] = 0;
    }
    if (state != 0) {
        UB_pickState = 0;
    }
    return state;
}
