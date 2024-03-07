//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_WEBVIEW_HEADER_INCLUDED
#define CHOC_WEBVIEW_HEADER_INCLUDED

#include <optional>
#include <unordered_map>
#include <vector>
#include <functional>
#include <unordered_set>
#include "../platform/choc_Platform.h"
#include "../text/choc_JSON.h"

//==============================================================================
namespace choc::ui
{

/**
    Creates an embedded browser which can be placed inside some kind of parent window.

    After creating a WebView object, its getViewHandle() returns a platform-specific
    handle that can be added to whatever kind of window is appropriate. The
    choc::ui::DesktopWindow class is an example of a window that can have the
    webview added to it via its choc::ui::DesktopWindow::setContent() method.

    There are unfortunately a few extra build steps needed for using WebView
    in your projects:

    - On OSX, you'll need to add the `WebKit` framework to your project

    - On Linux, you'll need to:
       1. Install the libgtk-3-dev and libwebkit2gtk-4.0-dev packages.
       2. Link the gtk+3.0 and webkit2gtk-4.0 libraries in your build.
          You might want to have a look inside choc/tests/CMakeLists.txt for
          an example of how to add those packages to your build without too
          much fuss.

    - On Windows, no extra build steps needed!! This is a bigger deal than it
      sounds, because normally to use an embedded Edge browser on Windows is a
      total PITA, involving downloading a special Microsoft SDK with redistributable
      DLLs, etc, but I've jumped through many ugly hoops to make this class
      fully self-contained.

    Because this is a GUI, it needs a message loop to be running. If you're using
    it inside an app which already runs a message loop, it should just work,
    or you can use choc::messageloop::run() and choc::messageloop::stop() for an easy
    but basic loop.

    For an example of how to use this class, see `choc/tests/main.cpp` where
    there's a simple demo.
*/
class WebView
{
public:
    /// Contains optional settings to pass to a WebView constructor.
    struct Options
    {
        /// If supported, this enables developer features in the browser
        bool enableDebugMode = false;

        /// On OSX, setting this to true will allow the first click on a non-focused
        /// webview to be used as input, rather than the default behaviour, which is
        /// for the first click to give the webview focus but not trigger any action.
        bool acceptsFirstMouseClick = false;

        /// Optional user-agent string which can be used to override the default. Leave
        // this empty for default behaviour.
        std::string customUserAgent;

        /// Serve resources to the browser from a C++ callback function.
        /// This can effectively be used to implement a basic web server,
        /// serving resources to the browser in any way the client code chooses.
        /// Given the path URL component (i.e. starting from "/"), the client should
        /// return some bytes, and the associated MIME type, for that resource.
        /// When provided, this function will initially be called with the root path
        /// ("/") in order to serve the initial content for the page.
        /// (i.e. the client should return some HTML with a "text/html" MIME type).
        /// Subsequent relative requests made from the page (i.e. via `img` tags,
        /// `fetch` calls from javascript etc) will result in subsequent calls here.
        struct Resource
        {
            std::vector<uint8_t> data;
            std::string mimeType;
        };

        using Path = std::string;
        using FetchResource = std::function<std::optional<Resource>(const Path&)>;
        FetchResource fetchResource;
    };

    /// Creates a WebView with default options
    WebView();
    /// Creates a WebView with some options
    WebView (const Options&);

    WebView (const WebView&) = delete;
    WebView (WebView&&) = default;
    WebView& operator= (WebView&&) = default;
    ~WebView();

    /// Returns true if the webview has been successfully initialised. This could
    /// fail on some systems if the OS doesn't provide a suitable component.
    bool loadedOK() const;

    /// Directly sets the HTML content of the browser
    bool setHTML (const std::string& html);

    /// Directly evaluates some javascript.
    /// This call isn't guaranteed to be thread-safe, so calling it on
    /// threads other than the main message thread could lead to problems.
    /// It also goes without saying this this is not realtime-safe, and the
    /// call may block, allocate or make system calls.
    bool evaluateJavascript (const std::string& script);

    /// Sends the browser to this URL
    bool navigate (const std::string& url);

    /// A callback function which can be passed to bind().
    using CallbackFn = std::function<choc::value::Value(const choc::value::ValueView& args)>;
    /// Binds a C++ function to a named javascript function that can be called
    /// by code running in the browser.
    bool bind (const std::string& functionName, CallbackFn&& function);
    /// Removes a previously-bound function.
    bool unbind (const std::string& functionName);

    /// Adds a script to run when the browser loads a page
    bool addInitScript (const std::string& script);

    /// Returns a platform-specific handle for this view
    void* getViewHandle() const;

    struct KeyListener {
        virtual void onKeyDown(std::string key) {}
        virtual void onKeyUp(std::string key) {}
    };

    void addKeyListener(KeyListener* l);
    void removeKeyListener(KeyListener* l);

private:
    //==============================================================================
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    std::unordered_map<std::string, CallbackFn> bindings;
    void invokeBinding (const std::string&);
};

} // namespace choc::ui


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

#if CHOC_LINUX

#include "../platform/choc_DisableAllWarnings.h"
#include <JavaScriptCore/JavaScript.h>
#include <webkit2/webkit2.h>
#include "../platform/choc_ReenableAllWarnings.h"

struct choc::ui::WebView::Pimpl
{
    Pimpl (WebView& v, const Options& options)
        : owner (v), fetchResource (options.fetchResource)
    {
        if (! gtk_init_check (nullptr, nullptr))
            return;

        webviewContext = webkit_web_context_new();
        g_object_ref_sink (G_OBJECT (webviewContext));
        webview = webkit_web_view_new_with_context (webviewContext);
        g_object_ref_sink (G_OBJECT (webview));
        manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (webview));

        signalHandlerID = g_signal_connect (manager, "script-message-received::external",
                                            G_CALLBACK (+[] (WebKitUserContentManager*, WebKitJavascriptResult* r, gpointer arg)
                                            {
                                                static_cast<Pimpl*> (arg)->invokeCallback (r);
                                            }),
                                            this);

        webkit_user_content_manager_register_script_message_handler (manager, "external");

        WebKitSettings* settings = webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview));
        webkit_settings_set_javascript_can_access_clipboard (settings, true);

        if (options.enableDebugMode)
        {
            webkit_settings_set_enable_write_console_messages_to_stdout (settings, true);
            webkit_settings_set_enable_developer_extras (settings, true);

            if (auto inspector = WEBKIT_WEB_INSPECTOR (webkit_web_view_get_inspector (WEBKIT_WEB_VIEW (webview))))
                webkit_web_inspector_show (inspector);
        }

        if (! options.customUserAgent.empty())
            webkit_settings_set_user_agent (settings, options.customUserAgent.c_str());

        if (options.fetchResource)
        {
            const auto onResourceRequested = [](auto* request, auto* context)
            {
                try
                {
                    const auto* path = webkit_uri_scheme_request_get_path (request);

                    if (const auto resource = static_cast<Pimpl*> (context)->fetchResource (path))
                    {
                        const auto& [bytes, mimeType] = *resource;

                        auto* streamBytes = g_bytes_new (bytes.data(), static_cast<gsize> (bytes.size()));
                        auto* stream = g_memory_input_stream_new_from_bytes (streamBytes);
                        g_bytes_unref (streamBytes);

                        auto* response = webkit_uri_scheme_response_new (stream, static_cast<gint64> (bytes.size()));
                        webkit_uri_scheme_response_set_status (response, 200, nullptr);
                        webkit_uri_scheme_response_set_content_type (response, mimeType.c_str());

                        auto* headers = soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
                        soup_message_headers_append (headers, "Cache-Control", "no-store");
                        soup_message_headers_append (headers, "Access-Control-Allow-Origin", "*");
                        webkit_uri_scheme_response_set_http_headers (response, headers); // response takes ownership of the headers

                        webkit_uri_scheme_request_finish_with_response (request, response);

                        g_object_unref (stream);
                        g_object_unref (response);
                    }
                    else
                    {
                        auto* stream = g_memory_input_stream_new();
                        auto* response = webkit_uri_scheme_response_new (stream, -1);
                        webkit_uri_scheme_response_set_status (response, 404, nullptr);

                        webkit_uri_scheme_request_finish_with_response (request, response);

                        g_object_unref (stream);
                        g_object_unref (response);
                    }
                }
                catch (...)
                {
                    auto* error = g_error_new (WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_FAILED, "Something went wrong");
                    webkit_uri_scheme_request_finish_error (request, error);
                    g_error_free (error);
                }
            };

            webkit_web_context_register_uri_scheme (webviewContext, "choc", onResourceRequested, this, nullptr);

            navigate ("choc://choc.choc/");
        }

        gtk_widget_show_all (webview);
    }

    ~Pimpl()
    {
        if (signalHandlerID != 0 && webview != nullptr)
            g_signal_handler_disconnect (manager, signalHandlerID);

        g_clear_object (&webview);
        g_clear_object (&webviewContext);
    }

    static constexpr const char* postMessageFn = "window.webkit.messageHandlers.external.postMessage";

    bool loadedOK() const           { return getViewHandle() != nullptr; }
    void* getViewHandle() const     { return (void*) webview; }

    bool evaluateJavascript (const std::string& js)
    {
       #if WEBKIT_CHECK_VERSION (2, 40, 0)
        webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (webview), js.c_str(), static_cast<gssize> (js.length()), nullptr, nullptr, nullptr, nullptr, nullptr);
       #else
        webkit_web_view_run_javascript (WEBKIT_WEB_VIEW (webview), js.c_str(), nullptr, nullptr, nullptr);
       #endif
        return true;
    }

    bool navigate (const std::string& url)
    {
        webkit_web_view_load_uri (WEBKIT_WEB_VIEW (webview), url.c_str());
        return true;
    }

    bool setHTML (const std::string& html)
    {
        webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview), html.c_str(), nullptr);
        return true;
    }

    bool addInitScript (const std::string& js)
    {
        if (manager != nullptr)
        {
            webkit_user_content_manager_add_script (manager, webkit_user_script_new (js.c_str(),
                                                                                     WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
                                                                                     WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                                                                                     nullptr, nullptr));
            return true;
        }

        return false;
    }

    void invokeCallback (WebKitJavascriptResult* r)
    {
        auto s = jsc_value_to_string (webkit_javascript_result_get_js_value (r));
        owner.invokeBinding (s);
        g_free (s);
    }

    WebView& owner;
    Options::FetchResource fetchResource;
    WebKitWebContext* webviewContext = {};
    GtkWidget* webview = {};
    WebKitUserContentManager* manager = {};
    unsigned long signalHandlerID = 0;
};

//==============================================================================
#elif CHOC_APPLE

#include "choc_MessageLoop.h"
#include "im_MacOS_Webview.h"

struct choc::ui::WebView::Pimpl
{
    Pimpl (WebView& v, const Options& optionsToUse)
        : owner (v), options (std::make_unique<Options> (optionsToUse))
    {
        using namespace choc::objc;
        CHOC_AUTORELEASE_BEGIN

        id config = call<id> (getClass ("WKWebViewConfiguration"), "new");

        id prefs = call<id> (config, "preferences");
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("fullScreenEnabled"));
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("DOMPasteAllowed"));
        call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("javaScriptCanAccessClipboard"));

        if (options->enableDebugMode)
            call<id> (prefs, "setValue:forKey:", getNSNumberBool (true), getNSString ("developerExtrasEnabled"));

        delegate = createDelegate();
        objc_setAssociatedObject (delegate, "choc_webview", (CHOC_OBJC_CAST_BRIDGED id) this, OBJC_ASSOCIATION_ASSIGN);

        manager = call<id> (config, "userContentController");
        call<void> (manager, "retain");
        call<void> (manager, "addScriptMessageHandler:name:", delegate, getNSString ("external"));

        if (options->fetchResource)
            call<void> (config, "setURLSchemeHandler:forURLScheme:", delegate, getNSString ("choc"));

        webview = call<id> (allocateWebview(), "initWithFrame:configuration:", CGRect(), config);
        objc_setAssociatedObject (webview, "choc_webview", (CHOC_OBJC_CAST_BRIDGED id) this, OBJC_ASSOCIATION_ASSIGN);

        if (! options->customUserAgent.empty())
            call<void> (webview, "setValue:forKey:", getNSString (options->customUserAgent), getNSString ("customUserAgent"));

        call<void> (webview, "setUIDelegate:", delegate);
        call<void> (webview, "setNavigationDelegate:", delegate);

        call<void> (config, "release");

        if (options->fetchResource)
            navigate ("choc://choc.choc/");

        CHOC_AUTORELEASE_END
    }

    ~Pimpl()
    {
        CHOC_AUTORELEASE_BEGIN
        objc_setAssociatedObject (delegate, "choc_webview", nil, OBJC_ASSOCIATION_ASSIGN);
        objc_setAssociatedObject (webview, "choc_webview", nil, OBJC_ASSOCIATION_ASSIGN);
        objc::call<void> (webview, "release");
        objc::call<void> (manager, "removeScriptMessageHandlerForName:", objc::getNSString ("external"));
        objc::call<void> (manager, "release");
        objc::call<void> (delegate, "release");
        CHOC_AUTORELEASE_END
    }

    static constexpr const char* postMessageFn = "window.webkit.messageHandlers.external.postMessage";

    bool loadedOK() const           { return getViewHandle() != nullptr; }
    void* getViewHandle() const     { return (CHOC_OBJC_CAST_BRIDGED void*) webview; }

    bool addInitScript (const std::string& script)
    {
        using namespace choc::objc;
        CHOC_AUTORELEASE_BEGIN

        if (id s = call<id> (call<id> (getClass ("WKUserScript"), "alloc"), "initWithSource:injectionTime:forMainFrameOnly:",
                                       getNSString (script), WKUserScriptInjectionTimeAtDocumentStart, (BOOL) 1))
        {
            call<void> (manager, "addUserScript:", s);
            call<void> (s, "release");
            return true;
        }

        CHOC_AUTORELEASE_END
        return false;
    }

    bool navigate (const std::string& url)
    {
        using namespace choc::objc;
        CHOC_AUTORELEASE_BEGIN

        if (id nsURL = call<id> (getClass ("NSURL"), "URLWithString:", getNSString (url)))
            return call<id> (webview, "loadRequest:", call<id> (getClass ("NSURLRequest"), "requestWithURL:", nsURL)) != nullptr;

        CHOC_AUTORELEASE_END
        return false;
    }

    bool setHTML (const std::string& html)
    {
        CHOC_AUTORELEASE_BEGIN
        return objc::call<id> (webview, "loadHTMLString:baseURL:", objc::getNSString (html), (id) nullptr) != nullptr;
        CHOC_AUTORELEASE_END
    }

    void setAcceptKeyEvents(bool accept) {
        objc::call<void>(webview, "setAcceptKeyEvents:", accept);
    }

    bool evaluateJavascript (const std::string& script)
    {
        CHOC_AUTORELEASE_BEGIN
        objc::call<void> (webview, "evaluateJavaScript:completionHandler:", objc::getNSString (script), (id) nullptr);
        return true;
        CHOC_AUTORELEASE_END
    }

private:
    id createDelegate()
    {
        static DelegateClass dc;
        return objc::call<id> ((id) dc.delegateClass, "new");
    }

    id allocateWebview();

    void onResourceRequested (void* task);

    BOOL sendAppAction (id self, const char* action)
    {
        objc::call<void> (objc::getSharedNSApplication(), "sendAction:to:from:",
                          sel_registerName (action), (id) nullptr, self);
        return true;
    }

    BOOL performKeyEquivalent (id self, id e)
    {
        enum
        {
            NSEventTypeKeyDown = 10,

            NSEventModifierFlagShift    = 1 << 17,
            NSEventModifierFlagControl  = 1 << 18,
            NSEventModifierFlagOption   = 1 << 19,
            NSEventModifierFlagCommand  = 1 << 20
        };

        if (objc::call<int> (e, "type") == NSEventTypeKeyDown)
        {
            auto flags = objc::call<int> (e, "modifierFlags") & (NSEventModifierFlagShift | NSEventModifierFlagCommand
                                                                 | NSEventModifierFlagControl | NSEventModifierFlagOption);

            auto path = objc::getString (objc::call<id> (e, "charactersIgnoringModifiers"));

            if (flags == NSEventModifierFlagCommand)
            {
                if (path == "c") return sendAppAction (self, "copy:");
                if (path == "x") return sendAppAction (self, "cut:");
                if (path == "v") return sendAppAction (self, "paste:");
                if (path == "z") return sendAppAction (self, "undo:");
                if (path == "a") return sendAppAction (self, "selectAll:");
            }
            else if (flags == (NSEventModifierFlagShift | NSEventModifierFlagCommand))
            {
                if (path == "Z") return sendAppAction (self, "redo:");
            }
        }

        return false;
    }

    void handleError (id error)
    {
        static constexpr int NSURLErrorCancelled = -999;

        if (objc::call<int> (error, "code") == NSURLErrorCancelled)
            return;

        auto errorString = objc::getString (objc::call<id> (error, "localizedDescription"));

        setHTML ("<!DOCTYPE html><html><head><title>Error</title></head>"
                 "<body><h2>" + errorString + "</h2></body></html>");
    }

    static Pimpl* getPimpl (id self)
    {
        return (CHOC_OBJC_CAST_BRIDGED Pimpl*) (objc_getAssociatedObject (self, "choc_webview"));
    }

    WebView& owner;
    // NB: this is a pointer because making it a member forces the alignment of this Pimpl class
    // to 16, which then conflicts with obj-C pointer alignment...
    std::unique_ptr<Options> options;
    id webview = {}, manager = {}, delegate = {};

    struct WebviewClass
    {
        WebviewClass();

        ~WebviewClass()
        {
            // NB: it doesn't seem possible to dispose of this class late enough to avoid a warning on shutdown
            // about the KVO system still using it, so unfortunately the only option seems to be to let it leak..
            // objc_disposeClassPair (webviewClass);
        }

        Class webviewClass = {};
    };

    struct DelegateClass
    {
        DelegateClass()
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
                                CHOC_AUTORELEASE_BEGIN
                                id panel = call<id> (getClass ("NSOpenPanel"), "openPanel");

                                auto allowsMultipleSelection = call<BOOL> (params, "allowsMultipleSelection");
                                id allowedFileExtensions = call<id> (params, "_allowedFileExtensions");
                                id window = call<id> (wkwebview, "window");

                                call<void> (panel, "setAllowsMultipleSelection:", allowsMultipleSelection);
                                call<void> (panel, "setAllowedFileTypes:", allowedFileExtensions);

                                call<void> (panel, "beginSheetModalForWindow:completionHandler:", window,
                                            ^(long result)
                                            {
                                                CHOC_AUTORELEASE_BEGIN
                                                if (result == 1) // NSModalResponseOK
                                                    completionHandler (call<id> (panel, "URLs"));
                                                else
                                                    completionHandler (nil);
                                                CHOC_AUTORELEASE_END
                                            });
                                CHOC_AUTORELEASE_END
                             }), "v@:@@@@");

            objc_registerClassPair (delegateClass);
        }

        ~DelegateClass()
        {
            objc_disposeClassPair (delegateClass);
        }

        Class delegateClass = {};
    };

    static constexpr long WKUserScriptInjectionTimeAtDocumentStart = 0;

    // Including CodeGraphics.h can create all kinds of messy C/C++ symbol clashes
    // with other headers, but all we actually need are these coordinate structs:
   #if defined (__LP64__) && __LP64__
    using CGFloat = double;
   #else
    using CGFloat = float;
   #endif

    struct CGPoint { CGFloat x = 0, y = 0; };
    struct CGSize  { CGFloat width = 0, height = 0; };
    struct CGRect  { CGPoint origin; CGSize size; };
};

//==============================================================================
#elif CHOC_WINDOWS

#include "../platform/choc_DynamicLibrary.h"

// If you want to supply your own mechanism for finding the Microsoft
// Webview2Loader.dll file, then define the CHOC_FIND_WEBVIEW2LOADER_DLL
// macro, which must evaluate to a choc::file::DynamicLibrary object
// which points to the DLL.
#ifndef CHOC_FIND_WEBVIEW2LOADER_DLL
 #define CHOC_USE_INTERNAL_WEBVIEW_DLL 1
 #define CHOC_FIND_WEBVIEW2LOADER_DLL choc::ui::getWebview2LoaderDLL()
 #include "../platform/choc_MemoryDLL.h"
 namespace choc::ui
 {
    using WebViewDLL = choc::memory::MemoryDLL;
    static WebViewDLL getWebview2LoaderDLL();
 }
#else
 namespace choc::ui
 {
    using WebViewDLL = choc::file::DynamicLibrary;
 }
#endif

#include "choc_DesktopWindow.h"

#ifndef __webview2_h__
#define __webview2_h__

#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include <atomic>
#include <shlobj.h>
#include <rpc.h>
#include <rpcndr.h>
#include <objidl.h>
#include <oaidl.h>

#ifndef __RPCNDR_H_VERSION__
#error "This code requires an updated version of <rpcndr.h>"
#endif

extern "C"
{

struct EventRegistrationToken { __int64 value; };

typedef interface ICoreWebView2 ICoreWebView2;
typedef interface ICoreWebView2Controller ICoreWebView2Controller;
typedef interface ICoreWebView2Environment ICoreWebView2Environment;
typedef interface ICoreWebView2HttpHeadersCollectionIterator ICoreWebView2HttpHeadersCollectionIterator;
typedef interface ICoreWebView2HttpRequestHeaders ICoreWebView2HttpRequestHeaders;
typedef interface ICoreWebView2HttpResponseHeaders ICoreWebView2HttpResponseHeaders;
typedef interface ICoreWebView2WebResourceRequest ICoreWebView2WebResourceRequest;
typedef interface ICoreWebView2WebResourceRequestedEventArgs ICoreWebView2WebResourceRequestedEventArgs;
typedef interface ICoreWebView2WebResourceRequestedEventHandler ICoreWebView2WebResourceRequestedEventHandler;
typedef interface ICoreWebView2WebResourceResponse ICoreWebView2WebResourceResponse;

MIDL_INTERFACE("4e8a3389-c9d8-4bd2-b6b5-124fee6cc14d")
ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT, ICoreWebView2Environment*) = 0;
};

MIDL_INTERFACE("6c4819f3-c9b7-4260-8127-c9f5bde7f68c")
ICoreWebView2CreateCoreWebView2ControllerCompletedHandler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT, ICoreWebView2Controller*) = 0;
};

MIDL_INTERFACE("b96d755e-0319-4e92-a296-23436f46a1fc")
ICoreWebView2Environment : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE CreateCoreWebView2Controller(HWND, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler*) = 0;
     virtual HRESULT STDMETHODCALLTYPE CreateWebResourceResponse(IStream*, int, LPCWSTR, LPCWSTR, ICoreWebView2WebResourceResponse**) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_BrowserVersionString(LPWSTR*) = 0;
     virtual HRESULT STDMETHODCALLTYPE add_NewBrowserVersionAvailable(void*, EventRegistrationToken*) = 0;
     virtual HRESULT STDMETHODCALLTYPE remove_NewBrowserVersionAvailable(EventRegistrationToken) = 0;
};

MIDL_INTERFACE("0f99a40c-e962-4207-9e92-e3d542eff849")
ICoreWebView2WebMessageReceivedEventArgs : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_WebMessageAsJson(LPWSTR *) = 0;
     virtual HRESULT STDMETHODCALLTYPE TryGetWebMessageAsString(LPWSTR *) = 0;
};

MIDL_INTERFACE("57213f19-00e6-49fa-8e07-898ea01ecbd2")
ICoreWebView2WebMessageReceivedEventHandler : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2 *, ICoreWebView2WebMessageReceivedEventArgs *) = 0;
};

enum COREWEBVIEW2_PERMISSION_KIND
{
    COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION = 0,
    COREWEBVIEW2_PERMISSION_KIND_MICROPHONE         = (COREWEBVIEW2_PERMISSION_KIND_UNKNOWN_PERMISSION + 1),
    COREWEBVIEW2_PERMISSION_KIND_CAMERA             = (COREWEBVIEW2_PERMISSION_KIND_MICROPHONE + 1),
    COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION        = (COREWEBVIEW2_PERMISSION_KIND_CAMERA + 1),
    COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS      = (COREWEBVIEW2_PERMISSION_KIND_GEOLOCATION + 1),
    COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS      = (COREWEBVIEW2_PERMISSION_KIND_NOTIFICATIONS + 1),
    COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ     = (COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS + 1)
};

enum COREWEBVIEW2_PERMISSION_STATE
{
    COREWEBVIEW2_PERMISSION_STATE_DEFAULT  = 0,
    COREWEBVIEW2_PERMISSION_STATE_ALLOW    = (COREWEBVIEW2_PERMISSION_STATE_DEFAULT + 1),
    COREWEBVIEW2_PERMISSION_STATE_DENY     = (COREWEBVIEW2_PERMISSION_STATE_ALLOW + 1)
};

enum COREWEBVIEW2_MOVE_FOCUS_REASON
{
    COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC = 0,
    COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT         = (COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC + 1),
    COREWEBVIEW2_MOVE_FOCUS_REASON_PREVIOUS     = (COREWEBVIEW2_MOVE_FOCUS_REASON_NEXT + 1)
};

enum COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT {};
enum COREWEBVIEW2_WEB_RESOURCE_CONTEXT
{
    COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL = 0
};

MIDL_INTERFACE("c10e7f7b-b585-46f0-a623-8befbf3e4ee0")
ICoreWebView2Deferral : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE Complete() = 0;
};

MIDL_INTERFACE("97055cd4-512c-4264-8b5f-e3f446cea6a5")
ICoreWebView2WebResourceRequest : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Uri(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Uri(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Method(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Method(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Content(IStream**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Content(IStream*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Headers(ICoreWebView2HttpRequestHeaders**) = 0;
};

MIDL_INTERFACE("973ae2ef-ff18-4894-8fb2-3c758f046810")
ICoreWebView2PermissionRequestedEventArgs : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE get_Uri(LPWSTR *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_PermissionKind(COREWEBVIEW2_PERMISSION_KIND *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_IsUserInitiated(BOOL *) = 0;
     virtual HRESULT STDMETHODCALLTYPE get_State(COREWEBVIEW2_PERMISSION_STATE *) = 0;
     virtual HRESULT STDMETHODCALLTYPE put_State(COREWEBVIEW2_PERMISSION_STATE) = 0;
     virtual HRESULT STDMETHODCALLTYPE GetDeferral(ICoreWebView2Deferral **) = 0;
};


MIDL_INTERFACE("e562e4f0-d7fa-43ac-8d71-c05150499f00")
ICoreWebView2Settings : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_IsScriptEnabled(BOOL * isScriptEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsScriptEnabled(BOOL isScriptEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsWebMessageEnabled(BOOL* isWebMessageEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsWebMessageEnabled(BOOL isWebMessageEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreDefaultScriptDialogsEnabled(BOOL* areDefaultScriptDialogsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreDefaultScriptDialogsEnabled(BOOL areDefaultScriptDialogsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsStatusBarEnabled(BOOL* isStatusBarEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsStatusBarEnabled(BOOL isStatusBarEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreDevToolsEnabled(BOOL* areDevToolsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreDevToolsEnabled(BOOL areDevToolsEnabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreDefaultContextMenusEnabled(BOOL* enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreDefaultContextMenusEnabled(BOOL enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AreHostObjectsAllowed(BOOL* allowed) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AreHostObjectsAllowed(BOOL allowed) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsZoomControlEnabled(BOOL* enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsZoomControlEnabled(BOOL enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsBuiltInErrorPageEnabled(BOOL* enabled) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsBuiltInErrorPageEnabled(BOOL enabled) = 0;
};

MIDL_INTERFACE("ee9a0f68-f46c-4e32-ac23-ef8cac224d2a")
ICoreWebView2Settings2 : public ICoreWebView2Settings
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_UserAgent(LPWSTR * userAgent) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_UserAgent(LPCWSTR userAgent) = 0;
};

MIDL_INTERFACE("15e1c6a3-c72a-4df3-91d7-d097fbec6bfd")
ICoreWebView2PermissionRequestedEventHandler : public IUnknown
{
public:
     virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs*) = 0;
};

MIDL_INTERFACE("76eceacb-0462-4d94-ac83-423a6793775e")
ICoreWebView2 : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Settings(ICoreWebView2Settings**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Source(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Navigate(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE NavigateToString(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationStarting(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationStarting(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContentLoading(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContentLoading(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_SourceChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_SourceChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_HistoryChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_HistoryChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NavigationCompleted(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NavigationCompleted(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationStarting(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationStarting(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_FrameNavigationCompleted(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_FrameNavigationCompleted(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ScriptDialogOpening(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ScriptDialogOpening(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PermissionRequested(ICoreWebView2PermissionRequestedEventHandler*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PermissionRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ProcessFailed(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ProcessFailed(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddScriptToExecuteOnDocumentCreated(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveScriptToExecuteOnDocumentCreated(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE ExecuteScript(LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE CapturePreview(COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT, void*, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE Reload() = 0;
    virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsJson(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE PostWebMessageAsString(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WebMessageReceived(ICoreWebView2WebMessageReceivedEventHandler*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WebMessageReceived(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE CallDevToolsProtocolMethod(LPCWSTR, LPCWSTR, void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_BrowserProcessId(UINT32*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanGoBack(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CanGoForward(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GoBack() = 0;
    virtual HRESULT STDMETHODCALLTYPE GoForward() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDevToolsProtocolEventReceiver(LPCWSTR, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop() = 0;
    virtual HRESULT STDMETHODCALLTYPE add_NewWindowRequested(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_NewWindowRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_DocumentTitleChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_DocumentTitleChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DocumentTitle(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddHostObjectToScript(LPCWSTR, VARIANT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveHostObjectFromScript(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE OpenDevToolsWindow() = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ContainsFullScreenElementChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ContainsFullScreenElementChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ContainsFullScreenElement(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WebResourceRequested(ICoreWebView2WebResourceRequestedEventHandler*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WebResourceRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE AddWebResourceRequestedFilter(const LPCWSTR, const COREWEBVIEW2_WEB_RESOURCE_CONTEXT) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveWebResourceRequestedFilter(const LPCWSTR, const COREWEBVIEW2_WEB_RESOURCE_CONTEXT) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_WindowCloseRequested(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_WindowCloseRequested(EventRegistrationToken) = 0;
};

MIDL_INTERFACE("4d00c0d1-9434-4eb6-8078-8697a560334f")
ICoreWebView2Controller : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_IsVisible(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsVisible(BOOL) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Bounds(RECT*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Bounds(RECT) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ZoomFactor(double*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ZoomFactor(double) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ZoomFactorChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ZoomFactorChanged(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBoundsAndZoomFactor(RECT, double) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_MoveFocusRequested(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_MoveFocusRequested(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_GotFocus(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_GotFocus(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_LostFocus(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_LostFocus(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_AcceleratorKeyPressed(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_AcceleratorKeyPressed(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ParentWindow(HWND*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ParentWindow(HWND) = 0;
    virtual HRESULT STDMETHODCALLTYPE NotifyParentWindowPositionChanged() = 0;
    virtual HRESULT STDMETHODCALLTYPE Close() = 0;
    virtual HRESULT STDMETHODCALLTYPE get_CoreWebView2(ICoreWebView2**) = 0;
};

STDAPI CreateCoreWebView2EnvironmentWithOptions(PCWSTR, PCWSTR, void*, ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler*);

MIDL_INTERFACE("0702fc30-f43b-47bb-ab52-a42cb552ad9f")
ICoreWebView2HttpHeadersCollectionIterator : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetCurrentHeader(LPWSTR*, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_HasCurrentHeader(BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveNext(BOOL*) = 0;
};

MIDL_INTERFACE("e86cac0e-5523-465c-b536-8fb9fc8c8c60")
ICoreWebView2HttpRequestHeaders : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE GetHeader(LPCWSTR, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHeaders(LPCWSTR, ICoreWebView2HttpHeadersCollectionIterator**) = 0;
    virtual HRESULT STDMETHODCALLTYPE Contains(LPCWSTR, BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetHeader(LPCWSTR, LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE RemoveHeader(LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIterator(ICoreWebView2HttpHeadersCollectionIterator**) = 0;
};

MIDL_INTERFACE("03c5ff5a-9b45-4a88-881c-89a9f328619c")
ICoreWebView2HttpResponseHeaders : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE AppendHeader(LPCWSTR, LPCWSTR) = 0;
    virtual HRESULT STDMETHODCALLTYPE Contains(LPCWSTR, BOOL*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHeader(LPCWSTR, LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetHeaders(LPCWSTR, ICoreWebView2HttpHeadersCollectionIterator**) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetIterator(ICoreWebView2HttpHeadersCollectionIterator**) = 0;
};

MIDL_INTERFACE("453e667f-12c7-49d4-be6d-ddbe7956f57a")
ICoreWebView2WebResourceRequestedEventArgs : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Request(ICoreWebView2WebResourceRequest**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Response(ICoreWebView2WebResourceResponse**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Response(ICoreWebView2WebResourceResponse*) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetDeferral(ICoreWebView2Deferral**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ResourceContext(COREWEBVIEW2_WEB_RESOURCE_CONTEXT*) = 0;
};

MIDL_INTERFACE("ab00b74c-15f1-4646-80e8-e76341d25d71")
ICoreWebView2WebResourceRequestedEventHandler : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Invoke(ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs*) = 0;
};

MIDL_INTERFACE("aafcc94f-fa27-48fd-97df-830ef75aaec9")
ICoreWebView2WebResourceResponse : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE get_Content(IStream**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Content(IStream*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Headers(ICoreWebView2HttpResponseHeaders**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_StatusCode(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_StatusCode(int) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ReasonPhrase(LPWSTR*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_ReasonPhrase(LPCWSTR) = 0;
};

}

#endif // __webview2_h__

namespace choc::ui
{

//==============================================================================
struct WebView::Pimpl
{
    Pimpl (WebView& v, const Options& opts)
        : owner (v), options (opts)
    {
        // You cam define this macro to provide a custom way of getting a
        // choc::file::DynamicLibrary that contains the redistributable
        // Microsoft WebView2Loader.dll
        webviewDLL = CHOC_FIND_WEBVIEW2LOADER_DLL;

        if (! webviewDLL)
            return;

        hwnd = windowClass.createWindow (WS_POPUP, 400, 400, this);

        if (hwnd.hwnd == nullptr)
            return;

        SetWindowLongPtr (hwnd, GWLP_USERDATA, (LONG_PTR) this);

        if (createEmbeddedWebView())
        {
            resizeContentToFit();
            // coreWebViewController->MoveFocus (COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    }

    ~Pimpl()
    {
        if (coreWebView != nullptr)
        {
            coreWebView->Release();
            coreWebView = nullptr;
        }

        if (coreWebViewController != nullptr)
        {
            coreWebViewController->Release();
            coreWebViewController = nullptr;
        }

        if (coreWebViewEnvironment != nullptr)
        {
            coreWebViewEnvironment->Release();
            coreWebViewEnvironment = nullptr;
        }
    }

    static constexpr const char* postMessageFn = "window.chrome.webview.postMessage";

    bool loadedOK() const           { return coreWebView != nullptr; }
    void* getViewHandle() const     { return (void*) hwnd.hwnd; }

    bool navigate (const std::string& url)
    {
        CHOC_ASSERT (coreWebView != nullptr);
        return coreWebView->Navigate (createUTF16StringFromUTF8 (url).c_str()) == S_OK;
    }

    bool addInitScript (const std::string& script)
    {
        CHOC_ASSERT (coreWebView != nullptr);
        return coreWebView->AddScriptToExecuteOnDocumentCreated (createUTF16StringFromUTF8 (script).c_str(), nullptr) == S_OK;
    }

    bool evaluateJavascript (const std::string& script)
    {
        CHOC_ASSERT (coreWebView != nullptr);
        return coreWebView->ExecuteScript (createUTF16StringFromUTF8 (script).c_str(), nullptr) == S_OK;
    }

    bool setHTML (const std::string& html)
    {
        CHOC_ASSERT (coreWebView != nullptr);

        pageHTML.data.clear();
        std::copy (html.begin(), html.end(), std::back_inserter (pageHTML.data));
        pageHTML.mimeType = "text/html";

        navigate (setHTMLUri);
        return true;
    }

    void addKeyListener(KeyListener* l) {
        keyListeners.insert(l);
    }

    void removeKeyListener(KeyListener* l) {
        keyListeners.erase(l);
    }

    void onJSKeyUp(const std::string& keyCode);
    void onJSKeyDown(const std::string& keyCode);

private:
    std::unordered_set<KeyListener*> keyListeners;

    WindowClass windowClass { L"CHOCWebView", (WNDPROC) wndProc };
    HWNDHolder hwnd;
    const std::string resourceRequestFilterUriPrefix = "https://choc.localhost/";
    const std::string setHTMLUri                     = "https://choc.localhost/setHTML";
    WebView::Options::Resource pageHTML;

    static Pimpl* getPimpl (HWND h)     { return (Pimpl*) GetWindowLongPtr (h, GWLP_USERDATA); }

    static LRESULT wndProc (HWND h, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (msg == WM_SIZE)
            if (auto w = getPimpl (h))
                w->resizeContentToFit();

        if (msg == WM_SHOWWINDOW)
            if (auto w = getPimpl (h); w->coreWebViewController != nullptr)
                w->coreWebViewController->put_IsVisible (wp != 0);

        return DefWindowProcW (h, msg, wp, lp);
    }

    void resizeContentToFit()
    {
        if (coreWebViewController != nullptr)
        {
            RECT r;
            GetWindowRect (hwnd, &r);
            r.right -= r.left; r.bottom -= r.top;
            r.left = r.top = 0;
            coreWebViewController->put_Bounds (r);
        }
    }

    bool createEmbeddedWebView()
    {
        if (auto userDataFolder = getUserDataFolder(); ! userDataFolder.empty())
        {
            auto handler = new EventHandler (*this);
            webviewInitialising.test_and_set();

            if (auto createCoreWebView2EnvironmentWithOptions = (decltype(&CreateCoreWebView2EnvironmentWithOptions))
                                                                   webviewDLL.findFunction ("CreateCoreWebView2EnvironmentWithOptions"))
            {
                if (createCoreWebView2EnvironmentWithOptions (nullptr, userDataFolder.c_str(), nullptr, handler) == S_OK)
                {
                    MSG msg;
                    auto timeoutTimer = SetTimer ({}, {}, 6000, {});

                    while (webviewInitialising.test_and_set() && GetMessage (std::addressof (msg), nullptr, 0, 0))
                    {
                        TranslateMessage (std::addressof (msg));
                        DispatchMessage (std::addressof (msg));

                        if (msg.message == WM_TIMER && msg.hwnd == nullptr && msg.wParam == timeoutTimer)
                            break;
                    }

                    KillTimer ({}, timeoutTimer);

                    if (coreWebView == nullptr)
                        return false;

                    const auto wildcardFilter = createUTF16StringFromUTF8 (resourceRequestFilterUriPrefix.c_str()) + L"*";
                    coreWebView->AddWebResourceRequestedFilter (wildcardFilter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);

                    EventRegistrationToken token;
                    coreWebView->add_WebResourceRequested (handler, std::addressof (token));

                    if (options.fetchResource)
                        navigate (resourceRequestFilterUriPrefix);

                    ICoreWebView2Settings* settings = nullptr;

                    if (coreWebView->get_Settings (std::addressof (settings)) == S_OK
                         && settings != nullptr)
                    {
                        settings->put_AreDevToolsEnabled (options.enableDebugMode);

                        if (! options.customUserAgent.empty())
                        {
                            ICoreWebView2Settings2* settings2 = nullptr;

                            // This palaver is needed because __uuidof doesn't work in MINGW
                            auto guid = IID { 0xee9a0f68, 0xf46c, 0x4e32, { 0xac, 0x23, 0xef, 0x8c, 0xac, 0x22, 0x4d, 0x2a } };

                            if (settings->QueryInterface (guid, (void**) std::addressof (settings2)) == S_OK
                                 && settings2 != nullptr)
                            {
                                auto agent = createUTF16StringFromUTF8 (options.customUserAgent);
                                settings2->put_UserAgent (agent.c_str());
                            }
                        }
                    }

                    return true;
                }
            }
        }

        return false;
    }

    bool environmentCreated (ICoreWebView2Environment* env)
    {
        if (coreWebViewEnvironment != nullptr)
            return false;

        env->AddRef();
        coreWebViewEnvironment = env;
        return true;
    }

    void webviewCreated (ICoreWebView2Controller* controller, ICoreWebView2* view)
    {
        if (controller != nullptr && view != nullptr)
        {
            controller->AddRef();
            view->AddRef();
            coreWebViewController = controller;
            coreWebView = view;
        }

        webviewInitialising.clear();
    }

    std::optional<WebView::Options::Resource> fetchResourceOrPageHTML (const std::string& uri)
    {
        if (uri == setHTMLUri)
            return pageHTML;

        return options.fetchResource (uri.substr (resourceRequestFilterUriPrefix.size() - 1));
    }

    HRESULT onResourceRequested (ICoreWebView2WebResourceRequestedEventArgs* args)
    {
        struct ScopedExit
        {
            using Fn = std::function<void()>;

            explicit ScopedExit (Fn&& fn) : onExit (std::move (fn)) {}

            ScopedExit (const ScopedExit&) = delete;
            ScopedExit (ScopedExit&&) = delete;
            ScopedExit& operator= (const ScopedExit&) = delete;
            ScopedExit& operator= (ScopedExit&&) = delete;

            ~ScopedExit()
            {
                if (onExit)
                    onExit();
            }

            Fn onExit;
        };

        auto makeCleanup          = [](auto*& ptr, auto cleanup) { return [&ptr, cleanup] { if (ptr) cleanup (ptr); }; };
        auto makeCleanupIUnknown  = [](auto*& ptr)               { return [&ptr]          { if (ptr) ptr->Release(); }; };

        try
        {
            if (coreWebViewEnvironment == nullptr)
                return E_FAIL;

            ICoreWebView2WebResourceRequest* request = {};
            ScopedExit cleanupRequest (makeCleanupIUnknown (request));

            if (args->get_Request (std::addressof (request)) != S_OK)
                return E_FAIL;

            LPWSTR uri = {};
            ScopedExit cleanupUri (makeCleanup (uri, CoTaskMemFree));

            if (request->get_Uri (std::addressof (uri)) != S_OK)
                return E_FAIL;

            ICoreWebView2WebResourceResponse* response = {};
            ScopedExit cleanupResponse (makeCleanupIUnknown (response));

            if (const auto resource = fetchResourceOrPageHTML (createUTF8FromUTF16 (uri)))
            {
                const auto makeMemoryStream = [](const auto* data, auto length) -> IStream*
                {
                    choc::file::DynamicLibrary lib ("shlwapi.dll");
                    using SHCreateMemStreamFn = IStream*(__stdcall *)(const BYTE*, UINT);
                    auto fn = reinterpret_cast<SHCreateMemStreamFn> (lib.findFunction ("SHCreateMemStream"));
                    return fn ? fn (data, length) : nullptr;
                };

                auto* stream = makeMemoryStream (reinterpret_cast<const BYTE*> (resource->data.data()),
                                                 static_cast<UINT> (resource->data.size()));

                if (stream == nullptr)
                    return E_FAIL;

                ScopedExit cleanupStream (makeCleanupIUnknown (stream));

                std::vector<std::string> headers;
                headers.emplace_back ("Content-Type: " + resource->mimeType);
                headers.emplace_back ("Cache-Control: no-store");
                headers.emplace_back ("Access-Control-Allow-Origin: *");

                if (! options.customUserAgent.empty())
                    headers.emplace_back ("User-Agent: " + options.customUserAgent);

                const auto headerString = createUTF16StringFromUTF8 (choc::text::joinStrings (headers, "\n"));

                if (coreWebViewEnvironment->CreateWebResourceResponse (stream, 200, L"OK", headerString.c_str(), std::addressof (response)) != S_OK)
                    return E_FAIL;
            }
            else
            {
                if (coreWebViewEnvironment->CreateWebResourceResponse (nullptr, 404, L"Not Found", nullptr, std::addressof (response)) != S_OK)
                    return E_FAIL;
            }

            if (args->put_Response (response) != S_OK)
                return E_FAIL;
        }
        catch (...)
        {
            return E_FAIL;
        }

        return S_OK;
    }

    //==============================================================================
    struct EventHandler  : public ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler,
                           public ICoreWebView2CreateCoreWebView2ControllerCompletedHandler,
                           public ICoreWebView2WebMessageReceivedEventHandler,
                           public ICoreWebView2PermissionRequestedEventHandler,
                           public ICoreWebView2WebResourceRequestedEventHandler
    {
        EventHandler (Pimpl& p) : ownerPimpl (p) {}
        EventHandler (const EventHandler&) = delete;
        EventHandler (EventHandler&&) = delete;
        EventHandler& operator= (const EventHandler&) = delete;
        EventHandler& operator= (EventHandler&&) = delete;
        virtual ~EventHandler() {}

        HRESULT STDMETHODCALLTYPE QueryInterface (REFIID, LPVOID*) override   { return E_NOINTERFACE; }
        ULONG STDMETHODCALLTYPE AddRef() override     { return ++refCount; }
        ULONG STDMETHODCALLTYPE Release() override    { auto newCount = --refCount; if (newCount == 0) delete this; return newCount; }

        HRESULT STDMETHODCALLTYPE Invoke (HRESULT, ICoreWebView2Environment* env) override
        {
            if (env == nullptr)
                return E_FAIL;

            if (! ownerPimpl.environmentCreated (env))
                return E_FAIL;

            env->CreateCoreWebView2Controller (ownerPimpl.hwnd, this);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (HRESULT, ICoreWebView2Controller* controller) override
        {
            if (controller == nullptr)
                return E_FAIL;

            ICoreWebView2* view = {};
            controller->get_CoreWebView2 (std::addressof (view));

            if (view == nullptr)
                return E_FAIL;

            EventRegistrationToken token;
            view->add_WebMessageReceived (this, std::addressof (token));
            view->add_PermissionRequested (this, std::addressof (token));
            ownerPimpl.webviewCreated (controller, view);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) override
        {
            if (sender == nullptr)
                return E_FAIL;

            LPWSTR message = {};
            args->TryGetWebMessageAsString (std::addressof (message));
            ownerPimpl.owner.invokeBinding (createUTF8FromUTF16 (message));
            sender->PostWebMessageAsString (message);
            CoTaskMemFree (message);
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* args) override
        {
            COREWEBVIEW2_PERMISSION_KIND permissionKind;
            args->get_PermissionKind (std::addressof (permissionKind));

            if (permissionKind == COREWEBVIEW2_PERMISSION_KIND_CLIPBOARD_READ)
                args->put_State (COREWEBVIEW2_PERMISSION_STATE_ALLOW);

            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Invoke (ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) override
        {
            return ownerPimpl.onResourceRequested (args);
        }

        Pimpl& ownerPimpl;
        std::atomic<unsigned long> refCount { 0 };
    };

    //==============================================================================
    WebView& owner;
    WebViewDLL webviewDLL;
    Options options;
    ICoreWebView2Environment* coreWebViewEnvironment = nullptr;
    ICoreWebView2* coreWebView = nullptr;
    ICoreWebView2Controller* coreWebViewController = nullptr;
    std::atomic_flag webviewInitialising = ATOMIC_FLAG_INIT;

    //==============================================================================
    static std::wstring getUserDataFolder()
    {
        wchar_t currentExePath[MAX_PATH] = {};
        wchar_t dataPath[MAX_PATH] = {};

        GetModuleFileNameW (nullptr, currentExePath, MAX_PATH);
        auto currentExeName = std::wstring (currentExePath);
        auto lastSlash = currentExeName.find_last_of (L'\\');

        if (lastSlash != std::wstring::npos)
            currentExeName = currentExeName.substr (lastSlash + 1);

        if (SUCCEEDED (SHGetFolderPathW (nullptr, CSIDL_APPDATA, nullptr, 0, dataPath)))
        {
            auto path = std::wstring (dataPath);

            if (! path.empty() && path.back() != L'\\')
                path += L"\\";

            return path + currentExeName;
        }

        return {};
    }
};

} // namespace choc::ui

#else
 #error "choc WebView only supports OSX, Windows or Linux!"
#endif

namespace choc::ui
{

//==============================================================================
inline WebView::WebView() : WebView (Options()) {}

inline WebView::WebView (const Options& options)
{
    pimpl = std::make_unique<Pimpl> (*this, options);

    if (! pimpl->loadedOK())
        pimpl.reset();

    bind("juce_enableKeyEvents", [&](const choc::value::ValueView &args) -> choc::value::Value {
#if JUCE_MAC
        pimpl->setAcceptKeyEvents(args[0].getWithDefault(false));
#endif
        return {};
    });

    bind("juce_onKeyDown", [&](const choc::value::ValueView &args) -> choc::value::Value {
#if JUCE_WINDOWS
        pimpl->onJSKeyDown(args[0].getWithDefault(""));
#endif
        return {};
    });

    bind("juce_onKeyUp", [&](const choc::value::ValueView &args) -> choc::value::Value {
#if JUCE_WINDOWS
        pimpl->onJSKeyUp(args[0].getWithDefault(""));
#endif
        return {};
    });

}

inline WebView::~WebView()
{
    pimpl.reset();
}

inline bool WebView::loadedOK() const                                { return pimpl != nullptr; }
inline bool WebView::navigate (const std::string& url)               { return pimpl != nullptr && pimpl->navigate (url.empty() ? "about:blank" : url); }
inline bool WebView::setHTML (const std::string& html)               { return pimpl != nullptr && pimpl->setHTML (html); }
inline bool WebView::addInitScript (const std::string& script)       { return pimpl != nullptr && pimpl->addInitScript (script); }
inline bool WebView::evaluateJavascript (const std::string& script)  { return pimpl != nullptr && pimpl->evaluateJavascript (script); }
inline void* WebView::getViewHandle() const                          { return pimpl != nullptr ? pimpl->getViewHandle() : nullptr; }

inline void WebView::addKeyListener(choc::ui::WebView::KeyListener *l) {
#if JUCE_WINDOWS
    if (pimpl != nullptr) pimpl->addKeyListener(l);
#endif
}

inline void WebView::removeKeyListener(choc::ui::WebView::KeyListener *l) {
#if JUCE_WINDOWS
    if (pimpl != nullptr) pimpl->removeKeyListener(l);
#endif
}

inline bool WebView::bind (const std::string& functionName, CallbackFn&& fn)
{
    if (pimpl == nullptr)
        return false;

    static constexpr std::string_view scriptTemplate = R"((function() {
const fnBinding = window._fnBindings = (window._fnBindings || { messageID: 1 });

window["FUNCTION_NAME"] = function()
{
  const messageID = ++fnBinding.messageID;
  const promise = new Promise((resolve, reject) => { fnBinding[messageID] = { resolve, reject }; });

  const args = JSON.stringify ({ id: messageID,
                                 fn: "FUNCTION_NAME",
                                 params: Array.prototype.slice.call (arguments)
                               },
                               (key, value) => typeof value === 'bigint' ? value.toString() : value);
  INVOKE_BINDING (args);
  return promise;
}
})())";

    auto script = choc::text::replace (scriptTemplate, "FUNCTION_NAME", functionName,
                                                       "INVOKE_BINDING", Pimpl::postMessageFn);

    if (addInitScript (script)
         && evaluateJavascript (script))
    {
        bindings[functionName] = std::move (fn);
        return true;
    }

    return false;
}

inline bool WebView::unbind (const std::string& functionName)
{
    if (auto found = bindings.find (functionName); found != bindings.end())
    {
        evaluateJavascript ("delete window[\"" + functionName + "\"];");
        bindings.erase (found);
        return true;
    }

    return false;
}

inline void WebView::invokeBinding (const std::string& msg)
{
    try
    {
        auto json = choc::json::parse (msg);
        auto b = bindings.find (std::string (json["fn"].getString()));
        auto callbackID = json["id"].getWithDefault<int64_t>(0);

        if (callbackID == 0 || b == bindings.end())
            return;

        auto callbackItem = "window._fnBindings[" + std::to_string (callbackID) + "]";

        try
        {
            auto result = b->second (json["params"]);

            auto call = callbackItem + ".resolve(" + choc::json::toString (result) + "); delete " + callbackItem + ";";
            evaluateJavascript (call);
            return;
        }
        catch (const std::exception&)
        {}

        auto call = callbackItem + ".reject(); delete " + callbackItem + ";";
        evaluateJavascript (call);
    }
    catch (const std::exception&)
    {}
}

} // namespace choc::ui


//==============================================================================
//==============================================================================
/*
    The data monstrosity that follows is a binary dump of the redistributable
    WebView2Loader.dll files from Microsoft, embedded here to avoid you
    needing to install the DLLs alongside your app.

    More details on how the microsoft webview2 packages work can be found at:
    https://docs.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution

    The copyright notice that goes with the redistributable files is
    printed below, and you should note that if you embed this code in your
    own app, you'll need to comply with this license!

    |
    |  Copyright (C) Microsoft Corporation. All rights reserved.
    |
    |  Redistribution and use in source and binary forms, with or without
    |  modification, are permitted provided that the following conditions are
    |  met:
    |
    |  * Redistributions of source code must retain the above copyright
    |  notice, this list of conditions and the following disclaimer.
    |  * Redistributions in binary form must reproduce the above
    |  copyright notice, this list of conditions and the following disclaimer
    |  in the documentation and/or other materials provided with the
    |  distribution.
    |  * The name of Microsoft Corporation, or the names of its contributors
    |  may not be used to endorse or promote products derived from this
    |  software without specific prior written permission.
    |
    |  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    |  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    |  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    |  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    |  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    |  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    |  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    |  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    |  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    |  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    |  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    |

*/

#if CHOC_WINDOWS
#include "choc_win_webview_hack.h"
#endif

#endif // CHOC_WEBVIEW_HEADER_INCLUDED
