//
// Created by pembe on 2/26/2024.
//

#include <iostream>
#include "choc_WebView.h"
#include <set>
#include <string>
#include <algorithm>

namespace choc::ui {

    // Helper function to check if a key is a MIDI keyboard key that should pass through to JUCE
    bool isMidiKeyboardKey(const std::string& keyCode) {
        // MIDI keyboard keys based on JUCE MidiKeyboardComponent default mapping: "awsedftgyhujkolp;"
        static const std::set<std::string> midiKeys = {
            "a", "w", "s", "e", "d", "f", "t", "g", "y", "h", "u", "j", "k", "o", "l", "p", ";"
        };

        std::string lowercaseKey = keyCode;
        std::transform(lowercaseKey.begin(), lowercaseKey.end(), lowercaseKey.begin(), ::tolower);

        return midiKeys.find(lowercaseKey) != midiKeys.end();
    }

    void WebView::Pimpl::onJSKeyDown(const std::string& keyCode) {
        // If keyboard events are enabled (e.g., typing in text input),
        // forward ALL keys to listeners (including MIDI keys)
        if (acceptKeyEvents) {
            for (auto l : keyListeners) {
                l->onKeyDown(keyCode);
            }
            return;
        }

        // When keyboard events are disabled, don't forward MIDI keyboard keys
        // to listeners - let them pass through to JUCE
        if (isMidiKeyboardKey(keyCode)) {
            return;
        }

        // Forward non-MIDI keys to listeners
        for (auto l : keyListeners) {
            l->onKeyDown(keyCode);
        }
    }

    void WebView::Pimpl::onJSKeyUp(const std::string& keyCode) {
        // If keyboard events are enabled (e.g., typing in text input),
        // forward ALL keys to listeners (including MIDI keys)
        if (acceptKeyEvents) {
            for (auto l : keyListeners) {
                l->onKeyUp(keyCode);
            }
            return;
        }

        // When keyboard events are disabled, don't forward MIDI keyboard keys
        // to listeners - let them pass through to JUCE
        if (isMidiKeyboardKey(keyCode)) {
            return;
        }

        // Forward non-MIDI keys to listeners
        for (auto l : keyListeners) {
            l->onKeyUp(keyCode);
        }
    }
}
