# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""U+FDFD is one terminal column with several cells of ink: the widest
ligature glyph in Unicode. The blanks behind the cluster catch that ink
the way they catch an italic shear; a strip cut at the old one-blank
boundary shows a two-cell stump instead of the ligature (issue 91)."""

import unittest

from font_fixture import AMIRI_FONT
from harness import Shitty


BORDER = 2
COLUMNS = 12
BISMILLAH = "﷽"


def render(text):
    with Shitty(
        columns=COLUMNS,
        rows=1,
        extra_arguments=("-fontsize", "20"),
    ) as terminal:
        terminal.write(b"\x1b[?25l" + text.encode())
        metrics = terminal.load_font(str(AMIRI_FONT))
        snapshot = terminal.snapshot()
        width, height, pixels = terminal.render_image(str(AMIRI_FONT))
    # When the image ends up blank, the failure message must say which
    # stage lost the ligature: the grid, the font, or the rasterizer.
    diagnostics = {
        "metrics": metrics,
        "cells": [
            (f"U+{ord(cell.char):04X}", int(cell.double_width))
            for cell in snapshot.cells[:6]
        ],
        "image": (width, height),
    }
    return width, height, pixels, diagnostics


def cell_ink(width, height, pixels, cell_width, cell):
    """The strongest ink of one cell against the background."""
    background = pixels[:3]
    value = 0
    for x in range(cell_width):
        for y in range(height - 2 * BORDER):
            offset = 3 * ((BORDER + y) * width + BORDER + cell * cell_width + x)
            value = max(value, max(
                abs(pixels[offset + at] - background[at]) for at in range(3)
            ))
    return value


def ink_profile(text):
    width, height, pixels, diagnostics = render(text)
    cell = (width - 2 * BORDER) // COLUMNS
    profile = [cell_ink(width, height, pixels, cell, at) for at in range(COLUMNS)]
    return profile, diagnostics


class WideLigatureOverflowTest(unittest.TestCase):
    def test_bismillah_ink_reaches_past_the_first_blank(self):
        profile, diagnostics = ink_profile(BISMILLAH)
        # The cluster cell and the first captured blank already ink.
        self.assertTrue(all(value > 64 for value in profile[:2]), (profile, diagnostics))
        # The ligature continues: the blanks behind them catch the rest
        # of the ink instead of clipping it at the strip edge.
        self.assertTrue(all(value > 32 for value in profile[2:4]), (profile, diagnostics))
        # The capture is bounded; far blanks stay blanks.
        self.assertTrue(all(value == 0 for value in profile[6:]), (profile, diagnostics))

    def test_a_neighbor_bounds_the_capture(self):
        # A printed cell is not a blank: the span cannot take it, and
        # the ligature clips at the span edge as before. The neighbor
        # keeps painting its own ink.
        profile, diagnostics = ink_profile(BISMILLAH + " A")
        self.assertTrue(profile[2] > 64, (profile, diagnostics))
        self.assertTrue(all(value == 0 for value in profile[3:]), (profile, diagnostics))

    def test_a_differently_painted_blank_bounds_the_capture(self):
        # The strip is a mask: ink crossing into a blank takes that
        # blank's own color, so a blank painting differently stays out
        # of the span and bounds the overflow.
        profile, diagnostics = ink_profile(BISMILLAH + "\x1b[31m \x1b[0m")
        self.assertTrue(profile[0] > 64, (profile, diagnostics))
        self.assertTrue(all(value == 0 for value in profile[1:]), (profile, diagnostics))


if __name__ == "__main__":
    unittest.main()
