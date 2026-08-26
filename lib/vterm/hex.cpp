/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "hex.h"

#include <std/str/fmt.h>
#include <std/ios/out_zc.h>

using namespace stl;

namespace stl {

    template <>
    void output<ZeroCopyOutput, ::Hex>(ZeroCopyOutput& output, ::Hex hex) {
        size_t digits = 1;
        for (u64 value = hex.value; value >= 16; value >>= 4) {
            ++digits;
        }

        const size_t length = hex.width > digits ? hex.width : digits;
        u8* const begin = static_cast<u8*>(output.imbue(length).ptr);
        u8* digit = begin;
        for (size_t index = digits; index < length; ++index) {
            *digit++ = u8'0';
        }
        u8* const end = static_cast<u8*>(formatU64Base16(hex.value, digit));
        if (hex.uppercase) {
            for (; digit != end; ++digit) {
                if (*digit >= u8'a' && *digit <= u8'f') {
                    *digit -= u8'a' - u8'A';
                }
            }
        }
        output.commit(length);
    }

}
