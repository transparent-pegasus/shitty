/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "point.h"

#include <std/str/view.h>
#include <std/sys/types.h>
#include <std/ios/out_zc.h>

using namespace stl;

Point::Point(int x_, int y_)
    : x(x_)
    , y(y_)
{
}

namespace stl {

    template <>
    void output<ZeroCopyOutput, ::Point>(ZeroCopyOutput& output, ::Point point) {
        output << StringView(u8"(") << (i64)(point.x) << StringView(u8",") << (i64)(point.y) << StringView(u8")");
    }

}
