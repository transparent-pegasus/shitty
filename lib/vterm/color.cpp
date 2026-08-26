/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "color.h"

#include "hex.h"

#include <std/str/view.h>
#include <std/ios/out_zc.h>

using namespace stl;

static_assert(sizeof(Color) == 4);

Color decHlsColor(u32 hue, u32 luminosity, u32 saturation) {
    hue %= 360;
    if (luminosity > 100) {
        luminosity = 100;
    }
    if (saturation > 100) {
        saturation = 100;
    }

    const float light = (float)(luminosity);
    const float sat = (float)(saturation);
    const float chroma = (50.0f - __builtin_fabsf(light - 50.0f)) * sat / 50.0f;
    const float second = chroma * (60.0f - __builtin_fabsf((float)(hue % 120) - 60.0f)) / 60.0f;
    const float offset = light - chroma / 2.0f;
    const float scale = 255.0f / 100.0f;
    const u8 firstComponent = (u8)((chroma + offset) * scale + 0.5f);
    const u8 secondComponent = (u8)((second + offset) * scale + 0.5f);
    const u8 thirdComponent = (u8)(offset * scale + 0.5f);

    if (hue < 60) {
        return {secondComponent, thirdComponent, firstComponent};
    }
    if (hue < 120) {
        return {firstComponent, thirdComponent, secondComponent};
    }
    if (hue < 180) {
        return {firstComponent, secondComponent, thirdComponent};
    }
    if (hue < 240) {
        return {secondComponent, firstComponent, thirdComponent};
    }
    if (hue < 300) {
        return {thirdComponent, firstComponent, secondComponent};
    }
    return {thirdComponent, secondComponent, firstComponent};
}

namespace stl {
    template <>
    void output<ZeroCopyOutput, ::Color>(ZeroCopyOutput& output, ::Color color) {
        output << StringView(u8"rgb:") << Hex{(u64)(color.red) * 0x101, 4} << StringView(u8"/") << Hex{(u64)(color.green) * 0x101, 4} << StringView(u8"/") << Hex{(u64)(color.blue) * 0x101, 4};
    }
}
