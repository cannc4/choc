#import "im_MacOS_Webview.h"
#include "choc_WebView.h"
#include "choc_MessageLoop.h"

@implementation imagiroWebView

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration {
    self = [super initWithFrame:frame configuration:configuration];
    if (self) {
        [self registerForDraggedTypes:@[(id)kUTTypeFileURL, NSPasteboardTypeFileURL, NSFilenamesPboardType]];
        acceptKeyEvents = NO;
    }
    return self;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
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
    if (acceptKeyEvents) {
        [super keyDown:event];
    } else {
        [[self nextResponder] keyDown:event];
    }
}

- (void)keyUp:(NSEvent *)event {
    if (acceptKeyEvents) {
        [super keyUp:event];
    } else {
        [[self nextResponder] keyUp:event];
    }
}

- (void)interpretKeyEvents:(NSArray<NSEvent*> *)events {
    [super interpretKeyEvents:events];
}

- (BOOL)performKeyEquivalent:(NSEvent *)event
{

    NSString *characters = [[event charactersIgnoringModifiers] lowercaseString];
    NSEventModifierFlags modifiers = [event modifierFlags];

    if (!acceptKeyEvents) {
        return NO;
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

    return [super performKeyEquivalent:event];
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
