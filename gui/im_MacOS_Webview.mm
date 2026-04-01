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

- (BOOL)performKeyEquivalent:(NSEvent *)event
{
    if (isHandlingKeyEquivalent || isHandlingKeyboardDispatch) {
        return NO;
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

    if ([characters isEqualToString:@"c"] && (modifiers & NSEventModifierFlagCommand))
    {
        // Handle copy action
        [self copy:self];
        return YES;
    }
    else if ([characters isEqualToString:@"x"] && (modifiers & NSEventModifierFlagCommand))
    {
        // Handle cut action
        [self cut:self];
        return YES;
    }
    else if ([characters isEqualToString:@"v"] && (modifiers & NSEventModifierFlagCommand))
    {
        // Handle paste action
        [self paste:self];
        return YES;
    }
    else if ([characters isEqualToString:@"a"] && (modifiers & NSEventModifierFlagCommand))
    {
        // Handle select all action
        [self selectAll:self];
        [self evaluateJavaScript:@"document.execCommand('selectAll')" completionHandler:nil];
        return YES;
    }
    else if ([characters isEqualToString:@"z"] && (modifiers & NSEventModifierFlagCommand))
    {
        if (modifiers & NSEventModifierFlagShift)
        {
            // Handle redo action
            [self evaluateJavaScript:@"document.execCommand('redo')" completionHandler:nil];
            return YES;
        }
        else
        {
            // Handle undo action
            [self evaluateJavaScript:@"document.execCommand('undo')" completionHandler:nil];
            return YES;
        }
        return YES;
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
