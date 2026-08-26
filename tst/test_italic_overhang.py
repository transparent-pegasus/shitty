# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""A run's ink is not bounded by the cell advance: an italic shear
reaches past the last cell, and the strip is only as wide as the run.
The blank behind the run catches it - when that blank paints ink the
way the run does."""

import unittest

from font_fixture import NERD_FONT
from harness import TEST_PLATFORM, Shitty


BORDER = 2


def render(text, columns=6):
    with Shitty(
        columns=columns,
        rows=1,
        extra_arguments=("-fontsize", "20"),
    ) as terminal:
        terminal.write(b"\x1b[?25l" + text.encode())
        terminal.load_font(str(NERD_FONT))
        return terminal.render_image(str(NERD_FONT))


def cell_columns(width, height, pixels, cell_width, cell):
    """Per-column ink of one cell, left to right."""
    background = pixels[:3]
    profile = []
    for x in range(cell_width):
        value = 0
        for y in range(height - 2 * BORDER):
            offset = 3 * ((BORDER + y) * width + BORDER + cell * cell_width + x)
            value = max(value, max(
                abs(pixels[offset + at] - background[at]) for at in range(3)
            ))
        profile.append(value)
    return profile


class ItalicOverhangTest(unittest.TestCase):
    def cell_width(self, width):
        return (width - 2 * BORDER) // 6

    def test_sheared_tail_lands_in_the_captured_blank(self):
        # Mid-run the shear of the first w paints the leading columns of
        # the next cell; at the end of a run that ink used to be clipped
        # away with the strip.
        width, height, mid = render("\x1b[3mww\x1b[0m")
        cell = self.cell_width(width)
        spill = cell_columns(width, height, mid, cell, 1)
        overhang = [value for value in spill[:2]]
        self.assertTrue(all(value > 64 for value in overhang), overhang)

        width, height, tail = render("\x1b[3mw \x1b[0m")
        captured = cell_columns(width, height, tail, cell, 1)
        self.assertEqual(captured[:2], overhang)
        # How many of the blank's columns the shear inks is the
        # backend's geometry: FreeType's oblique dies within two
        # columns - keep that pinned - while CoreText shears farther.
        # On both, the capture is bounded: the next cell stays a blank.
        if TEST_PLATFORM != "cocoa":
            self.assertTrue(all(value == 0 for value in captured[2:]), captured)
        beyond = cell_columns(width, height, tail, cell, 2)
        self.assertTrue(all(value == 0 for value in beyond), (captured, beyond))

    def test_a_bold_blank_still_catches_the_tail(self):
        # A blank shapes to nothing, so an attribute that only picks a
        # face - the space before a bold word carries the bold - must not
        # keep the cell out of the run.
        width, height, plain = render("\x1b[3mww\x1b[0m")
        cell = self.cell_width(width)
        overhang = cell_columns(width, height, plain, cell, 1)[:2]

        width, height, pixels = render("\x1b[3mw\x1b[1m \x1b[0m")
        captured = cell_columns(width, height, pixels, cell, 1)
        self.assertEqual(captured[:2], overhang)

    def test_a_differently_painted_blank_catches_nothing(self):
        # The strip is a mask, so the crossing pixels would take the
        # blank's own color; a blank that paints differently is left out
        # of the run and the tail is clipped as before.
        width, height, pixels = render("\x1b[3mw\x1b[31m \x1b[0m")
        cell = self.cell_width(width)
        captured = cell_columns(width, height, pixels, cell, 1)
        self.assertTrue(all(value == 0 for value in captured), captured)


if __name__ == "__main__":
    unittest.main()
