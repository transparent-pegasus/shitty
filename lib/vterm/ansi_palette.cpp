/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "ansi_palette.h"

bool AnsiPalette::operator==(const AnsiPalette& rhs) const noexcept {
    for (size_t index = 0; index < colorCount; ++index) {
        if (!(colors[index] == rhs.colors[index])) {
            return false;
        }
    }
    return true;
}
