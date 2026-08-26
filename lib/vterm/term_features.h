/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

namespace stl {
    class StringBuilder;
}

class UnicodeWidths;

// Appends the iTerm2 feature-reporting string, one alphanumeric run naming
// every capability we answer for (https://iterm2.com/feature-reporting/).
// Applications see it two ways: as the OSC 1337;Capabilities query reply
// and as TERM_FEATURES in every child's environment. widths becomes
// the Uw field.
void appendTermFeatures(stl::StringBuilder& target, const UnicodeWidths& widths);
