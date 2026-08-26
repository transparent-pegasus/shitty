/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "rect.h"

#include <std/str/view.h>
#include <std/ios/out_zc.h>

using namespace stl;

Rect::Rect(Point topLeft, Point bottomRight)
    : tl(topLeft)
    , br(bottomRight)
{
}

Rect::Rect(int x, int y)
    : tl(x, y)
    , br(x + 1, y)
{
}

Rect::Rect(int x1, int y1, int x2, int y2)
    : tl(x1, y1)
    , br(x2, y2)
{
}

void Rect::clear() {
    tl = Point();
    br = Point();
}

namespace stl {

    template <>
#if __SIZEOF_LONG_DOUBLE__ >= 16
    void output<ZeroCopyOutput, ::Rect>(ZeroCopyOutput& output, ::Rect rect) {
#else
    void output<ZeroCopyOutput, ::Rect>(ZeroCopyOutput& output, const ::Rect& rect) {
#endif
        output << StringView(u8"Rect{tl=") << rect.tl << StringView(u8" br=") << rect.br << (rect.rectangular ? StringView(u8" rectangular}") : StringView(u8" regular}"));
    }

}
