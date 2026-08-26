/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "term_features.h"

#include <lib/vterm/unicode_width.h>

#include <std/str/view.h>
#include <std/str/builder.h>

using namespace stl;

void appendTermFeatures(StringBuilder& target, const UnicodeWidths& widths) {
    // Every code answers for a capability the terminal actually has; the
    // deliberate omissions are Aw (ambiguous-width characters stay narrow)
    // and File (no iTerm2 inline-image protocol).
    //
    //   T3    SGR 38/48;2 direct color, semicolon form and the colon form
    //         with a colorspace field
    //   Cw    OSC 52 clipboard write
    //   Lr    DECLRMM/DECSLRM left-right margins
    //   M     mouse reporting (DECSET 1000/1002/1003/1006)
    //   Sc7   DECSCUSR cursor styles 0 through 6
    //   U     UTF-8 and basic Unicode
    //   Uw<N> Unicode version the cell widths emulate (-unicodeWidths)
    //   Ts3   title setting and the title stack (XTWINOPS 22/23)
    //   B     bracketed paste (DECSET 2004)
    //   F     focus reporting (DECSET 1004)
    //   Gs    SGR 9 strikethrough
    //   Go    SGR 53 overline
    //   Sy    synchronized output (DECSET 2026)
    //   H     OSC 8 hyperlinks
    //   No    OSC 9 notifications
    //   Sx    sixel graphics
    //   P     OSC 9;4 progress
    target << StringView(u8"T3CwLrMSc7UUw") << widths.level() << StringView(u8"Ts3BFGsGoSyHNoSxP");
}
