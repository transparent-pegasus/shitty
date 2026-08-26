/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include "point.h"

struct Rect {
    Point tl;
    Point br;
    bool rectangular = false;

    Rect() = default;
    Rect(Point topLeft, Point bottomRight);
    Rect(int x, int y);
    Rect(int x1, int y1, int x2, int y2);

    bool null() const {
        return tl == Point() && br == Point();
    }

    bool empty() const {
        return tl == br;
    }

    Point mid() const {
        return Point((tl.x + br.x) / 2, (tl.y + br.y) / 2);
    }

    void clear();

    void toggleRectangular() {
        rectangular = !rectangular;
    }
};
