/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "render.h"

#include <lib/vterm/terminal_types.h>

#include <std/tst/ut.h>

using namespace stl;

STD_TEST_SUITE(Renderer) {
    STD_TEST(PacksCellAttributeBits) {
        TerminalCell cell{};
        cell.bold = true;
        cell.italic = true;
        cell.underline_style = 1;
        cell.inverse = true;
        cell.wrap = true;
        cell.faint = true;
        cell.blink = true;
        cell.conceal = true;
        cell.strike = true;
        cell.overline = true;
        cell.underline_style = 5;
        cell.dwidth = true;
        cell.dwidth_cont = true;

        const u32 expected = (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 8) | (1u << 9) | (1u << 10) | (1u << 11) | (1u << 12) | (5u << 13) | (1u << 16) | (1u << 17);
        STD_INSIST(Renderer::cellAttributes(cell) == expected);
    }
}
