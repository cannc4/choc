//
// Created by pembe on 2/26/2024.
//

#include <iostream>
#include "choc_WebView.h"

namespace choc::ui {
    void WebView::Pimpl::onJSKeyDown(const std::string& keyCode) {
        for (auto l : keyListeners) {
            l->onKeyDown(keyCode);
        }
    }

    void WebView::Pimpl::onJSKeyUp(const std::string& keyCode) {
        for (auto l : keyListeners) {
            l->onKeyUp(keyCode);
        }
    }
}
