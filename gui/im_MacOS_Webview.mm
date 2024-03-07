#import "im_MacOS_Webview.h"
#include "choc_WebView.h"
#include "choc_MessageLoop.h"

@implementation imagiroWebView

- (instancetype)initWithFrame:(CGRect)frame configuration:(WKWebViewConfiguration *)configuration {
    self = [super initWithFrame:frame configuration:configuration];
    if (self) {
        [self registerForDraggedTypes:@[NSFilenamesPboardType]];
        acceptKeyEvents = NO;
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

        class_addMethod (webviewClass, sel_registerName ("acceptsFirstMouse:"),
                         (IMP) (+[](id self, SEL, id) -> BOOL
                         {
                             if (auto p = getPimpl (self))
                                 return p->options->acceptsFirstMouseClick;

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
                        @"Cache-Control": @"no-cache",
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
