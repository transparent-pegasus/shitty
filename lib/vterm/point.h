/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

struct Point {
    int x = -1;
    int y = -1;

    Point() = default;
    Point(int x, int y);

    bool operator<(const Point& rhs) const {
        return y < rhs.y || (y == rhs.y && x < rhs.x);
    }

    bool operator==(const Point& rhs) const {
        return x == rhs.x && y == rhs.y;
    }

    bool operator<=(const Point& rhs) const {
        return operator<(rhs) || operator==(rhs);
    }
};
