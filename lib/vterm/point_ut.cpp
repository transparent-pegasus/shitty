/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "point.h"

#include <std/tst/ut.h>
#include <std/str/view.h>
#include <std/str/builder.h>

using namespace stl;

STD_TEST_SUITE(Point) {
    STD_TEST(OrdersByRowThenColumn) {
        STD_INSIST(Point(5, 1) < Point(0, 2));
        STD_INSIST(Point(1, 1) < Point(2, 1));
        STD_INSIST(Point(2, 1) <= Point(2, 1));
        STD_INSIST(!(Point(2, 1) < Point(1, 1)));
    }

    STD_TEST(FormatsCoordinates) {
        StringBuilder output;
        output << Point(2, 3);

        STD_INSIST(StringView(output) == StringView(u8"(2,3)"));
    }
}
