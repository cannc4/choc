
#ifdef __OBJC__

#import <WebKit/WebKit.h>

@interface imagiroWebView : WKWebView <WKNavigationDelegate, WKUIDelegate, NSDraggingDestination> {
    BOOL acceptKeyEvents;
    BOOL debugMode;
}

- (void)setAcceptKeyEvents:(BOOL)accept;

@end

#endif