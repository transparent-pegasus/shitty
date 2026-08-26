/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <lib/vterm/terminal_types.h>

#include <std/sys/types.h>

#include <stddef.h>

namespace stl {
    class ObjPool;
}

struct Composer;

// One rendered span of a row in view units: cells [begin, end) map onto
// slices of one strip at offset in the plane's arena; slice i of cell
// begin + i starts at offset + i * cellWidth with the strip width as the
// row stride. missing marks an uncovered cluster with no strip.
struct ScreenRowSpan {
    u16 begin = 0;
    u16 end = 0;
    u32 offset = 0;
    bool color = false;
    bool missing = false;
};

// The render side of the cell grid: cuts a row into same-font spans,
// shapes and rasterizes them through the composer's fontpack into strip
// arenas, and caches everything content-addressably - rows under the
// model's row identity, strips under the shaped text. It knows nothing
// of the screen: callers hand it cells and an identity, and a mutated
// row simply arrives under a fresh one. When an arena outgrows its
// budget the shaper resets outright - rows, strips and arenas die, the
// generation moves, and the visible content reshapes once on the next
// pull, exactly like a font change.
struct SpanShaper {
    // Cuts, dedupes and rasterizes the row's spans on demand; out must
    // hold a full row of entries. Returns 0 when the composer has no
    // fontpack. Rendered strips live in the span arenas below,
    // append-only between resets.
    virtual size_t rowSpans(const TerminalCell* cells, u16 columns, u64 rowId, ScreenRowSpan* out) = 0;
    // Shapes a run of cells outside any row (the preedit overlay)
    // through the same caches and arenas; the returned spans use
    // baseColumn-relative view columns and stay valid within the
    // current spanGeneration().
    virtual size_t shapeCells(const TerminalCell* cells, u16 count, u16 baseColumn, ScreenRowSpan* out) = 0;
    // Changes whenever the strip offsets move wholesale (reset, font
    // change). Values never repeat, so a renderer that keyed its device
    // copies on one generation and later sees another knows to
    // re-upload - and to pull every visible row again.
    virtual u32 spanGeneration() const = 0;
    virtual const u8* spanMask() const = 0;
    virtual size_t spanMaskUsed() const = 0;
    virtual const u32* spanColor() const = 0;
    virtual size_t spanColorUsed() const = 0;

    static SpanShaper* create(Composer& composer, stl::ObjPool& pool);
};
