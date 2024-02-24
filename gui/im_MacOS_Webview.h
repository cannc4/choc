
#ifdef __OBJC__

#import <WebKit/WebKit.h>

@interface imagiroWebView : WKWebView <WKNavigationDelegate, WKUIDelegate, NSDraggingDestination>
@end

#endif