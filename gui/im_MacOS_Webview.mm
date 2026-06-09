#import "im_MacOS_Webview.h"
#include "choc_WebView.h"
#include "choc_MessageLoop.h"

@implementation imagiroWebView

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration {
    self = [super initWithFrame:frame configuration:configuration];
    if (self) {
        [self registerForDraggedTypes:@[(id)kUTTypeFileURL, NSPasteboardTypeFileURL, NSFilenamesPboardType]];
        acceptKeyEvents = NO;
        debugMode = NO;
        isHandlingKeyEquivalent = NO;
        isHandlingKeyEvent = NO;
        isHandlingKeyboardDispatch = NO;
    }
    return self;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (void)setDebugMode:(BOOL)enabled {
    debugMode = enabled;
}

- (void)setAcceptKeyEvents:(BOOL)accept {
    acceptKeyEvents = accept;
}

- (void)willOpenMenu:(NSMenu *)menu withEvent:(NSEvent *)event {
    if (!debugMode)
        [menu removeAllItems];
    else
        [super willOpenMenu:menu withEvent:event];
}

// Helper method to check if a key is a MIDI keyboard key that should pass through to JUCE
- (BOOL)isMidiKeyboardKey:(NSString *)characters {
    // MIDI keyboard keys based on JUCE MidiKeyboardComponent default mapping: "awsedftgyhujkolp;"
    NSSet *midiKeys = [NSSet setWithObjects:@"a", @"w", @"s", @"e", @"d", @"f", @"t", @"g", @"y", @"h", @"u", @"j", @"k", @"o", @"l", @"p", @";", nil];
    NSString *lowercaseKey = [characters lowercaseString];
    return [midiKeys containsObject:lowercaseKey];
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

    return [[[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding] autorelease];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    // Check if this is a file drag
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if (!filePaths || filePaths.count == 0) {
        return [super draggingEntered:sender];
    }

    NSString *jsonString = [self jsonStringForFilePaths:filePaths];
    NSPoint dragPoint = [sender draggingLocation];

    // Convert coordinates from window space to view space
    NSPoint localPoint = [self convertPoint:dragPoint fromView:nil];

    NSString *jsCode = [NSString stringWithFormat:@"if (window.ui && typeof window.ui.handleDragEnter === 'function') { window.ui.handleDragEnter(%@, %f, %f) }", jsonString, localPoint.x, localPoint.y];
    [self evaluateJavaScript:jsCode completionHandler:^(id result, NSError *error) {
        if (error) {
            NSLog(@"Drag enter error: %@", error);
        }
    }];

    return NSDragOperationCopy;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
    // Check if this is a file drag
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if (!filePaths || filePaths.count == 0) {
        [super draggingExited:sender];
        return;
    }

    NSString *jsCode = @"if (window.ui && typeof window.ui.handleDragLeave === 'function') { window.ui.handleDragLeave() }";
    [self evaluateJavaScript:jsCode completionHandler:^(id result, NSError *error) {
        if (error) {
            NSLog(@"Drag exit error: %@", error);
        }
    }];

    [super draggingExited:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
    // Check if this is a file drag
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if (!filePaths || filePaths.count == 0) {
        return [super draggingUpdated:sender];
    }

    NSString *jsonString = [self jsonStringForFilePaths:filePaths];
    NSPoint dragPoint = [sender draggingLocation];

    // Convert coordinates from window space to view space
    NSPoint localPoint = [self convertPoint:dragPoint fromView:nil];

    NSString *jsCode = [NSString stringWithFormat:@"if (window.ui && typeof window.ui.handleDragOver === 'function') { window.ui.handleDragOver(%@, %f, %f) }", jsonString, localPoint.x, localPoint.y];
    [self evaluateJavaScript:jsCode completionHandler:^(id result, NSError *error) {
        if (error) {
            NSLog(@"Drag over error: %@", error);
        }
    }];

    return NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    // Check if this is a file drag
    NSArray *filePaths = [[sender draggingPasteboard] propertyListForType:NSFilenamesPboardType];
    if (!filePaths || filePaths.count == 0) {
        return [super performDragOperation:sender];
    }

    NSString *jsonString = [self jsonStringForFilePaths:filePaths];
    NSPoint dragPoint = [sender draggingLocation];

    // Convert coordinates from window space to view space
    NSPoint localPoint = [self convertPoint:dragPoint fromView:nil];

    NSString *jsCode = [NSString stringWithFormat:@"if (window.ui && typeof window.ui.handleDragDrop === 'function') { window.ui.handleDragDrop(%@, %f, %f) }", jsonString, localPoint.x, localPoint.y];
    [self evaluateJavaScript:jsCode completionHandler:^(id result, NSError *error) {
        if (error) {
            NSLog(@"Drag drop error: %@", error);
        }
    }];

    return YES;
}

- (void)keyDown:(NSEvent *)event {
    if (isHandlingKeyEvent || isHandlingKeyboardDispatch) {
        return;
    }

    NSString *characters = [event charactersIgnoringModifiers];

    // If acceptKeyEvents is true, let the WebView handle ALL keys (including MIDI keys)
    // This allows typing in text inputs, textareas, etc.
    if (acceptKeyEvents) {
        isHandlingKeyboardDispatch = YES;
        isHandlingKeyEvent = YES;
        @try { [super keyDown:event]; }
        @finally {
            isHandlingKeyEvent = NO;
            isHandlingKeyboardDispatch = NO;
        }
        return;
    }

    // When acceptKeyEvents is false, pass MIDI keyboard keys through to JUCE
    // but let WebView handle other keys that the app might need (like arrow keys, escape, etc.)
    if ([self isMidiKeyboardKey:characters]) {
        isHandlingKeyboardDispatch = YES;
        @try { [[self nextResponder] keyDown:event]; }
        @finally { isHandlingKeyboardDispatch = NO; }
        return;
    }

    // For non-MIDI keys when acceptKeyEvents is false, let WebView handle them
    // so the app can respond to navigation keys, etc.
    isHandlingKeyboardDispatch = YES;
    isHandlingKeyEvent = YES;
    @try { [super keyDown:event]; }
    @finally {
        isHandlingKeyEvent = NO;
        isHandlingKeyboardDispatch = NO;
    }
}

- (void)keyUp:(NSEvent *)event {
    if (isHandlingKeyEvent || isHandlingKeyboardDispatch) {
        return;
    }

    NSString *characters = [event charactersIgnoringModifiers];

    // If acceptKeyEvents is true, let the WebView handle ALL keys (including MIDI keys)
    // This allows typing in text inputs, textareas, etc.
    if (acceptKeyEvents) {
        isHandlingKeyboardDispatch = YES;
        isHandlingKeyEvent = YES;
        @try { [super keyUp:event]; }
        @finally {
            isHandlingKeyEvent = NO;
            isHandlingKeyboardDispatch = NO;
        }
        return;
    }

    // When acceptKeyEvents is false, pass MIDI keyboard keys through to JUCE
    // but let WebView handle other keys that the app might need
    if ([self isMidiKeyboardKey:characters]) {
        isHandlingKeyboardDispatch = YES;
        @try { [[self nextResponder] keyUp:event]; }
        @finally { isHandlingKeyboardDispatch = NO; }
        return;
    }

    isHandlingKeyboardDispatch = YES;
    isHandlingKeyEvent = YES;
    @try { [super keyUp:event]; }
    @finally {
        isHandlingKeyEvent = NO;
        isHandlingKeyboardDispatch = NO;
    }
}

- (void)interpretKeyEvents:(NSArray<NSEvent*> *)events {
    [super interpretKeyEvents:events];
}

// Dispatch a synthetic JS KeyboardEvent. If JS does NOT call preventDefault(),
// run the native fallback on the main queue.
- (void)dispatchJSKeyEvent:(NSString *)key code:(NSString *)code shiftKey:(BOOL)shift fallback:(dispatch_block_t)fallback
{
    NSString *js = [NSString stringWithFormat:
        @"(function(){"
         "var e=new KeyboardEvent('keydown',{key:'%@',code:'%@',metaKey:true,shiftKey:%@,bubbles:true,cancelable:true});"
         "return window.dispatchEvent(e)"
         "})();",
        key, code, shift ? @"true" : @"false"];
    [self evaluateJavaScript:js completionHandler:^(id result, NSError *error) {
        if (!error && [result boolValue] && fallback) {
            dispatch_async(dispatch_get_main_queue(), fallback);
        }
    }];
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if (isHandlingKeyEquivalent || isHandlingKeyboardDispatch) {
        return YES; // Consume the event to break WebKit ↔ AppKit key event recursion cycle
    }

    NSString *characters = [[event charactersIgnoringModifiers] lowercaseString];
    NSEventModifierFlags modifiers = [event modifierFlags];

    if (!acceptKeyEvents) {
        return NO;
    }

    // Consume Enter/Return and Escape when we have keyboard focus
    // This prevents these keys from propagating to the DAW (e.g., Logic's transport controls)
    unsigned short keyCode = [event keyCode];
    if (keyCode == 36 || keyCode == 76) {  // Return or numpad Enter
        isHandlingKeyboardDispatch = YES;
        isHandlingKeyEquivalent = YES;
        isHandlingKeyEvent = YES;
        @try { [super keyDown:event]; }
        @finally {
            isHandlingKeyEvent = NO;
            isHandlingKeyEquivalent = NO;
            isHandlingKeyboardDispatch = NO;
        }
        return YES;
    }
    if (keyCode == 53) {  // Escape
        isHandlingKeyboardDispatch = YES;
        isHandlingKeyEquivalent = YES;
        isHandlingKeyEvent = YES;
        @try { [super keyDown:event]; }
        @finally {
            isHandlingKeyEvent = NO;
            isHandlingKeyEquivalent = NO;
            isHandlingKeyboardDispatch = NO;
        }
        return YES;
    }

    // For modifier-key combos, dispatch a synthetic JS KeyboardEvent first so PAM's
    // JS handlers (undo/redo, sequence copy/paste, select-all) can intercept them.
    // dispatchEvent() returns true if NO handler called preventDefault() (= JS didn't handle it),
    // and false if a handler DID call preventDefault() (= JS handled it).
    // So: native fallback fires only when dispatchEvent returns true (JS passed).

    if ([characters isEqualToString:@"c"] && (modifiers & NSEventModifierFlagCommand))
    {
        [self dispatchJSKeyEvent:@"c" code:@"KeyC" shiftKey:NO fallback:^{ [self copy:self]; }];
        return YES;
    }
    else if ([characters isEqualToString:@"x"] && (modifiers & NSEventModifierFlagCommand))
    {
        [self dispatchJSKeyEvent:@"x" code:@"KeyX" shiftKey:NO fallback:^{ [self cut:self]; }];
        return YES;
    }
    else if ([characters isEqualToString:@"v"] && (modifiers & NSEventModifierFlagCommand))
    {
        [self dispatchJSKeyEvent:@"v" code:@"KeyV" shiftKey:NO fallback:^{ [self paste:self]; }];
        return YES;
    }
    else if ([characters isEqualToString:@"a"] && (modifiers & NSEventModifierFlagCommand))
    {
        [self dispatchJSKeyEvent:@"a" code:@"KeyA" shiftKey:NO fallback:^{ [self selectAll:self]; }];
        return YES;
    }
    else if ([characters isEqualToString:@"z"] && (modifiers & NSEventModifierFlagCommand))
    {
        if (modifiers & NSEventModifierFlagShift)
        {
            [self dispatchJSKeyEvent:@"z" code:@"KeyZ" shiftKey:YES fallback:^{
                [self evaluateJavaScript:@"document.execCommand('redo')" completionHandler:nil];
            }];
            return YES;
        }
        else
        {
            [self dispatchJSKeyEvent:@"z" code:@"KeyZ" shiftKey:NO fallback:^{
                [self evaluateJavaScript:@"document.execCommand('undo')" completionHandler:nil];
            }];
            return YES;
        }
    }

    isHandlingKeyboardDispatch = YES;
    isHandlingKeyEquivalent = YES;
    BOOL result;
    @try { result = [super performKeyEquivalent:event]; }
    @finally {
        isHandlingKeyEquivalent = NO;
        isHandlingKeyboardDispatch = NO;
    }
    return result;
}

// getUserMedia permission is granted on choc's CHOCWebViewDelegate_ (choc_WebView.h),
// the object actually assigned as the WKWebView's UIDelegate. A copy here would be
// dead code — imagiroWebView is never its own UIDelegate.

@end

namespace choc::ui {

    id WebView::Pimpl::allocateWebview()
    {
        static WebviewClass c;
        return objc::call<id> ((id)objc_getClass("imagiroWebView"), "alloc");
    }

    WebView::Pimpl::WebviewClass::WebviewClass()
    {
        webviewClass = choc::objc::createDelegateClass ("imagiroWebView", "CHOCWebView_");

        objc_registerClassPair (webviewClass);
    }


    void WebView::Pimpl::onResourceRequested(void* taskPtr)
    {
        auto task = (__bridge id<WKURLSchemeTask>)taskPtr;

        @try
        {
            NSURL* requestUrl = task.request.URL;

            auto makeResponse = [&](NSInteger responseCode, NSDictionary* mutableHeaderFields)
            {
                NSHTTPURLResponse* response = [[[NSHTTPURLResponse alloc] initWithURL:requestUrl
                                                                           statusCode:responseCode
                                                                          HTTPVersion:@"HTTP/1.1"
                                                                         headerFields:mutableHeaderFields] autorelease];
                return response;
            };


            NSString* path = requestUrl.path;
            std::string pathStr = path.UTF8String;

            if (auto resource = options->fetchResource(pathStr))
            {
                const auto& [bytes, mimeType] = *resource;

                NSString* contentLength = [NSString stringWithFormat:@"%lu", bytes.size()];
                NSString* mimeTypeNS = [NSString stringWithUTF8String:mimeType.c_str()];
                NSDictionary* headerFields = @{
                        @"Content-Length": contentLength,
                        @"Content-Type": mimeTypeNS,
                        @"Cache-Control": @"no-store",
                        @"Access-Control-Allow-Origin": @"*",
                };

                [task didReceiveResponse:makeResponse(200, headerFields)];

                NSData* data = [NSData dataWithBytes:bytes.data() length:bytes.size()];
                [task didReceiveData:data];
            }
            else
            {
                [task didReceiveResponse:makeResponse(404, nil)];
            }

            [task didFinish];
        }
        @catch (...)
        {
            NSError* error = [NSError errorWithDomain:NSURLErrorDomain code:-1 userInfo:nil];
            [task didFailWithError:error];
        }

    }


};
