/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "vt_geometry.h"

#include "vt_host.h"

#include <std/alg/minmax.h>
#include <std/dbg/assert.h>

using namespace stl;

void VtGeometry::setCellPixelSize(u16 width, u16 height) {
    STD_ASSERT(width != 0);
    STD_ASSERT(height != 0);
    if (cellPixelWidth == width && cellPixelHeight == height) {
        return;
    }
    cellPixelWidth = width;
    cellPixelHeight = height;
}

void VtGeometry::resize(u16 pixelWidth_, u16 pixelHeight_, VtHost* host) {
    STD_ASSERT(cellPixelWidth != 0);
    STD_ASSERT(cellPixelHeight != 0);

    const u32 borders = 2u * borderPixels;
    const u32 contentWidth = pixelWidth_ > borders ? pixelWidth_ - borders : 0;
    const u32 contentHeight = pixelHeight_ > borders ? pixelHeight_ - borders : 0;
    const u16 columns_ = (u16)(max<u32>(1, contentWidth / cellPixelWidth));
    const u16 rows_ = (u16)(max<u32>(1, contentHeight / cellPixelHeight));

    if (columns == columns_ && rows == rows_ && pixelWidth == pixelWidth_ && pixelHeight == pixelHeight_) {
        return;
    }

    columns = columns_;
    rows = rows_;
    pixelWidth = pixelWidth_;
    pixelHeight = pixelHeight_;

    if (host != nullptr) {
        host->resized();
    }
}
