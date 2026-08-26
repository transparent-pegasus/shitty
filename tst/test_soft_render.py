# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""-soft: unhinted rendering with subpixel placement and stem darkening
scaled by the option value."""

import unittest

from font_fixture import NERD_FONT
from harness import TEST_PLATFORM, Shitty, run_startup_failure


def render(soft, text):
    arguments = ("-fontsize", "16")
    if soft is not None:
        arguments += ("-soft", str(soft))
    with Shitty(columns=len(text), rows=1, extra_arguments=arguments) as terminal:
        terminal.write(b"\x1b[?25l" + text.encode())
        terminal.load_font(str(NERD_FONT))
        width, height, pixels = terminal.render_image(str(NERD_FONT))
        return width, height, bytes(pixels)


def ink(width, height, pixels):
    background = pixels[:3]
    total = 0
    for offset in range(0, len(pixels), 3):
        total += max(
            abs(pixels[offset + at] - background[at]) for at in range(3)
        )
    return total


# -soft drives the FreeType rasterizer; on cocoa the CoreText renderer
# sits first in the chain and the option has nothing to steer.
@unittest.skipIf(TEST_PLATFORM == "cocoa", "CoreText renders here; -soft is a FreeType knob")
class SoftRenderTest(unittest.TestCase):
    def test_soft_zero_departs_from_the_hinted_grid(self):
        classic = render(None, "Hamburg")
        soft = render(0, "Hamburg")
        self.assertEqual(classic[:2], soft[:2])
        self.assertNotEqual(classic[2], soft[2])

    def test_darkening_scales_with_the_option(self):
        light = render(0, "Hamburg")
        dark = render(100, "Hamburg")
        self.assertEqual(light[:2], dark[:2])
        self.assertGreater(ink(*dark), ink(*light))


class SoftOptionTest(unittest.TestCase):
    def test_out_of_range_values_fail_loudly(self):
        for value in ("101", "-2", "x"):
            with self.subTest(value=value):
                result = run_startup_failure(extra_arguments=("-soft", value))
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(b"-soft", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
