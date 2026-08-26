/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include "color.h"

namespace stl {
    class StringView;
}

// The X11 rgb.txt lookup: case-insensitive, both spellings ("alice blue"
// and "AliceBlue"). Backs named colors in OSC color specifications.
bool colorFromName(stl::StringView name, Color& color);
