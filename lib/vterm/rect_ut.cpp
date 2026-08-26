/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "rect.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/str/builder.h>

using namespace stl;

STD_TEST_SUITE(Rect) {
    STD_TEST(ConstructsAndClears) {
        Rect cell(3, 4);
        STD_INSIST(cell.tl == Point(3, 4));
        STD_INSIST(cell.br == Point(4, 4));
        STD_INSIST(!cell.empty());
        STD_INSIST(cell.mid() == Point(3, 4));

        cell.toggleRectangular();
        STD_INSIST(cell.rectangular);
        cell.clear();
        STD_INSIST(cell.null());
        STD_INSIST(cell.rectangular);
    }

    STD_TEST(FormatsBoundsAndMode) {
        StringBuilder output;
        output << Rect(1, 2, 3, 4);

        STD_INSIST(StringView(output) == StringView(u8"Rect{tl=(1,2) br=(3,4) regular}"));
    }
}
