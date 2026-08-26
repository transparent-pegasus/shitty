/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "ansi_palette.h"

#include <std/tst/ut.h>

using namespace stl;

STD_TEST_SUITE(AnsiPalette) {
    STD_TEST(ProvidesIndexedAccessAndValueEquality) {
        AnsiPalette left;
        AnsiPalette right;
        left[3] = {1, 2, 3};
        right.colors[3] = {1, 2, 3};

        STD_INSIST(left == right);

        right[15] = {4, 5, 6};
        STD_INSIST(!(left == right));
    }
}
