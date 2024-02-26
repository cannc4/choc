#import "im_MacOS_Webview.h"
#include "choc_WebView.h"
#include "choc_MessageLoop.h"

@implementation imagiroWebView

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration {
    self = [super initWithFrame:frame configuration:configuration];
    if (self) {
        [self registerForDraggedTypes:@[NSFilenamesPboardType]];
        acceptKeyEvents = YES;
    }
    return self;
}

- (void)setAcceptKeyEvents:(BOOL)accept {
    acceptKeyEvents = accept;
}

- (NSString *)jsonStringForFilePaths:(NSArray *)filePaths {
    NSError *error;
    NSData *jsonData = [NSJSONSerialization dataWithJSONObject:filePaths
                                                       options:0
                                                         error:&error];
    if (!jsonData) {
        NSLog(@"Failed to serialize file paths to JSON: %@", error);
        return @"[]"; // Return an empty array representation in case of error
    }

    return [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    NSString *jsonString = [self jsonStringForFilePaths:filePaths];
    NSString *jsCode = [NSString stringWithFormat:@"window.ui.handleDragEnter(%@)", jsonString];
    [self evaluateJavaScript:jsCode completionHandler:nil];
    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    NSString *jsCode = @"window.ui.handleDragLeave()";
    [self evaluateJavaScript:jsCode completionHandler:nil];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    NSString *jsonString = [self jsonStringForFilePaths:filePaths];
    NSString *jsCode = [NSString stringWithFormat:@"window.ui.handleDragOver(%@)", jsonString];
    [self evaluateJavaScript:jsCode completionHandler:nil];
    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    NSString *jsonString = [self jsonStringForFilePaths:filePaths];
    NSString *jsCode = [NSString stringWithFormat:@"window.ui.handleDragDrop(%@)", jsonString];
    [self evaluateJavaScript:jsCode completionHandler:nil];
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    if (acceptKeyEvents) [super keyDown:event];
    else [[self nextResponder] keyDown:event];
}

- (void)keyUp:(NSEvent *)event {
    if (acceptKeyEvents) [super keyUp:event];
    else [[self nextResponder] keyUp:event];
}

- (void)interpretKeyEvents:(NSArray<NSEvent*> *)events {
    [super interpretKeyEvents:events];
}

@end

namespace choc::ui {

    WebView::Pimpl::Pimpl (WebView& v, const Options& optionsToUse)
            : owner (v), options (optionsToUse)
    {
        using namespace choc::objc;
        AutoReleasePool autoreleasePool;

        id config = call<id> (getClass ("WKWebViewConfiguration"), "new");

        id prefs = call<id> (config, "preferences");
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("fullScreenEnabled"));
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("DOMPasteAllowed"));
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("javaScriptCanAccessClipboard"));

        if (options.enableDebugMode)
            call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("developerExtrasEnabled"));

        delegate = createDelegate();
        objc_setAssociatedObject (delegate, "choc_webview", (id) this, OBJC_ASSOCIATION_ASSIGN);

        manager = call<id> (config, "userContentController");
        call<void> (manager, "retain");
        call<void> (manager, "addScriptMessageHandler:name:", delegate, getNSString ("external"));

        if (options.fetchResource)
            call<void> (config, "setURLSchemeHandler:forURLScheme:", delegate, getNSString ("choc"));

        webview = call<id> (allocateWebview(), "initWithFrame:configuration:", CGRect(), config);
        objc_setAssociatedObject (webview, "choc_webview", (id) this, OBJC_ASSOCIATION_ASSIGN);

        if (! options.customUserAgent.empty())
            call<void> (webview, "setValue:forKey:", getNSString (options.customUserAgent), getNSString ("customUserAgent"));

        call<void> (webview, "setUIDelegate:", delegate);
        call<void> (webview, "setNavigationDelegate:", delegate);

        call<void> (config, "release");

        if (options.fetchResource)
            navigate ("choc://choc.choc/");

        owner.bind("juce_enableKeyEvents", [&](const choc::value::ValueView &args) -> choc::value::Value {
            call<void>(webview, "setAcceptKeyEvents:", args[0].getWithDefault(false));
            return {};
        });
    }

    WebView::Pimpl::~Pimpl()
    {
        objc::AutoReleasePool autoreleasePool;
        objc_setAssociatedObject (delegate, "choc_webview", nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject (webview, "choc_webview", nil, OBJC_ASSOCIATION_ASSIGN);
        objc::call<void> (webview, "release");
        objc::call<void> (manager, "removeScriptMessageHandlerForName:", objc::getNSString ("external"));
        objc::call<void> (manager, "release");
        objc::call<void> (delegate, "release");
    }

    bool WebView::Pimpl::evaluateJavascript (const std::string& script)
    {
        objc::AutoReleasePool autoreleasePool;
        objc::call<void> (webview, "evaluateJavaScript:completionHandler:", objc::getNSString (script), (id) nullptr);
        return true;
    }

    id WebView::Pimpl::allocateWebview()
    {
        static WebviewClass c;
        return objc::call<id> ((id)objc_getClass("imagiroWebView"), "alloc");
    }

    WebView::Pimpl::WebviewClass::WebviewClass()
    {
        webviewClass = choc::objc::createDelegateClass ("imagiroWebView", "CHOCWebView_");

        class_addMethod (webviewClass, sel_registerName ("acceptsFirstMouse:"),
                         (IMP) (+[](id self, SEL, id) -> BOOL
                         {
                             if (auto p = getPimpl (self))
                                 return p->options.acceptsFirstMouseClick;

                             return false;
                         }), "B@:@");

        class_addMethod (webviewClass, sel_registerName ("performKeyEquivalent:"),
                         (IMP) (+[](id self, SEL, id e) -> BOOL
                         {
                             if (auto p = getPimpl (self))
                                 return p->performKeyEquivalent (self, e);

                             return false;
                         }), "B@:@");

        objc_registerClassPair (webviewClass);
    }

    WebView::Pimpl::WebviewClass::~WebviewClass()
    {
        // NB: it doesn't seem possible to dispose of this class late enough to avoid a warning on shutdown
        // about the KVO system still using it, so unfortunately the only option seems to be to let it leak..
        // objc_disposeClassPair (webviewClass);
    }

    Class webviewClass = {};

    WebView::Pimpl::DelegateClass::DelegateClass()
    {
        using namespace choc::objc;
        delegateClass = createDelegateClass ("NSObject", "CHOCWebViewDelegate_");

        class_addMethod (delegateClass, sel_registerName ("userContentController:didReceiveScriptMessage:"),
                         (IMP) (+[](id self, SEL, id, id msg)
                         {
                             if (auto p = getPimpl (self))
                                 p->owner.invokeBinding (objc::getString (call<id> (msg, "body")));
                         }),
                         "v@:@@");

        class_addMethod (delegateClass, sel_registerName ("webView:startURLSchemeTask:"),
                         (IMP) (+[](id self, SEL, id, id task)
                         {
                             if (auto p = getPimpl (self))
                                 p->onResourceRequested (task);
                         }),
                         "v@:@@");

        class_addMethod (delegateClass, sel_registerName ("webView:didFailProvisionalNavigation:withError:"),
                         (IMP) (+[](id self, SEL, id, id, id error)
                         {
                             if (auto p = getPimpl (self))
                                 p->handleError (error);
                         }),
                         "v@:@@@");

        class_addMethod (delegateClass, sel_registerName ("webView:didFailNavigation:withError:"),
                         (IMP) (+[](id self, SEL, id, id, id error)
                         {
                             if (auto p = getPimpl (self))
                                 p->handleError (error);
                         }),
                         "v@:@@@");

        class_addMethod (delegateClass, sel_registerName ("webView:stopURLSchemeTask:"), (IMP) (+[](id, SEL, id, id) {}), "v@:@@");

        class_addMethod (delegateClass, sel_registerName ("webView:runOpenPanelWithParameters:initiatedByFrame:completionHandler:"),
                         (IMP) (+[](id, SEL, id wkwebview, id params, id /*frame*/, void (^completionHandler)(id))
                         {
                             AutoReleasePool autoreleasePool;

                             id panel = call<id> (getClass ("NSOpenPanel"), "openPanel");

                             auto allowsMultipleSelection = call<BOOL> (params, "allowsMultipleSelection");
                             id allowedFileExtensions = call<id> (params, "_allowedFileExtensions");
                             id window = call<id> (wkwebview, "window");

                             call<void> (panel, "setAllowsMultipleSelection:", allowsMultipleSelection);
                             call<void> (panel, "setAllowedFileTypes:", allowedFileExtensions);

                             call<void> (panel, "beginSheetModalForWindow:completionHandler:", window,
                                         ^(long result)
                                         {
                                             AutoReleasePool pool;

                                             if (result == 1) // NSModalResponseOK
                                                 completionHandler (call<id> (panel, "URLs"));
                                             else
                                                 completionHandler (nil);
                                         });
                         }), "v@:@@@@");

        objc_registerClassPair (delegateClass);
    }

    WebView::Pimpl::DelegateClass::~DelegateClass()
    {
        objc_disposeClassPair (delegateClass);
    }

    Class delegateClass = {};
};
