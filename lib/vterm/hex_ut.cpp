/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "hex.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/str/builder.h>

using namespace stl;

STD_TEST_SUITE(Hex) {
    STD_TEST(FormatsWidthAndCase) {
        StringBuilder output;
        output << Hex{0} << StringView(u8" ") << Hex{0x2a, 4} << StringView(u8" ") << Hex{0xabcdef, 2, true};

        STD_INSIST(StringView(output) == StringView(u8"0 002a ABCDEF"));
    }

    STD_TEST(WidthNeverTruncatesValue) {
        StringBuilder output;
        output << Hex{0x12345, 2};

        STD_INSIST(StringView(output) == StringView(u8"12345"));
    }
}
