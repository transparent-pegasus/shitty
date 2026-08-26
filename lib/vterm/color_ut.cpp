/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "color.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/str/builder.h>

using namespace stl;

STD_TEST_SUITE(Color) {
    STD_TEST(FormatsSixteenBitComponents) {
        StringBuilder output;
        output << Color{0x12, 0x34, 0xab};

        STD_INSIST(StringView(output) == StringView(u8"rgb:1212/3434/abab"));
    }
}
