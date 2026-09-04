
#ifdef __OBJC__

#import <WebKit/WebKit.h>

@interface imagiroWebView : WKWebView <WKNavigationDelegate, WKUIDelegate, NSDraggingDestination> {
    BOOL acceptKeyEvents;
    BOOL debugMode;
    BOOL isHandlingKeyEquivalent;
    BOOL isHandlingKeyEvent;
    BOOL isHandlingKeyboardDispatch;
    NSString* dropStagingDirectory;
    NSOperationQueue* promiseQueue;
}

- (void)setAcceptKeyEvents:(BOOL)accept;
- (void)setFileDropStagingDirectory:(NSString *)path;

@end

#endif