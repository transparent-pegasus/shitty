/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */
/* part of this file is part of Zutty.
 * Copyright (C) 2020 Tom Szilagyi
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * See the file LICENSE.GPL3 for the full license.
 */

#include "terminal_types.h"

#include <std/dbg/assert.h>

static_assert(sizeof(CellColor) == 4, "CellColor size mismatch");
static_assert(sizeof(TerminalCell) == 16, "TerminalCell size mismatch");
static_assert(offsetof(TerminalCell, content) == 8, "TerminalCell content offset mismatch");
static_assert(offsetof(TerminalCell, payload) == 12, "TerminalCell payload offset mismatch");
static_assert(__is_trivial(TerminalCell), "TerminalCell must remain trivial");
static_assert(__is_trivially_copyable(TerminalCell), "TerminalCell must remain trivially copyable");
static_assert(__is_standard_layout(TerminalCell), "TerminalCell must remain standard-layout");

void TerminalCell::setExtraRef(u32 ref) noexcept {
    STD_ASSERT(ref <= maxExtraRef);
    extended = ref != 0;
    payload = ref == 0 ? 0 : (ref << 8) | extraRefSentinel;
}

Color TerminalColors::resolveForegroundSpecial(const TerminalCell& cell) const {
    const bool overrideAnsi = (specialModes & (1u << 5)) != 0;
    const CellColor foreground = cell.foreground();
    if (overrideAnsi || foreground.source() == CellColor::Source::DefaultForeground) {
        if ((specialModes & (1u << 2)) != 0 && cell.blink) {
            return special[2];
        }
        if ((specialModes & (1u << 0)) != 0 && cell.bold) {
            return special[0];
        }
        if ((specialModes & (1u << 1)) != 0 && cell.underlined()) {
            return special[1];
        }
        if ((specialModes & (1u << 4)) != 0 && cell.italic) {
            return special[4];
        }
    }
    return resolve(foreground);
}

Color TerminalColors::resolveBackgroundSpecial(const TerminalCell& cell) const {
    const bool overrideAnsi = (specialModes & (1u << 5)) != 0;
    const CellColor background = cell.background();
    if ((overrideAnsi || background.source() == CellColor::Source::DefaultBackground) && (specialModes & (1u << 3)) != 0 && cell.inverse) {
        return special[3];
    }
    return resolve(background);
}
