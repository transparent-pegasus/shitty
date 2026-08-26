# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import unittest

from harness import Shitty


CONTROL = 2
SUPER = 8
LEFT_CONTROL = 341
LEFT_SUPER = 343
PRESS = 1
RELEASE = 0


def cell_pixels(image, width, border, cell_width, cell_height, column):
    result = bytearray()
    left = border + column * cell_width
    for y in range(border, border + cell_height):
        begin = 3 * (y * width + left)
        result.extend(image[begin : begin + 3 * cell_width])
    return bytes(result)


class HyperlinkInputTest(unittest.TestCase):
    def test_stationary_pointer_tracks_control_content_focus_and_presence(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(
                b"\x1b[?25l"
                b"\x1b]8;id=old;https://old.test\x1b\\"
                b"ab"
                b"\x1b]8;;\x1b\\"
            )
            terminal.pointer(2, 2)
            self.assertEqual(terminal.desktop_state()["icon"], 0)

            terminal.frontend_key_event(
                LEFT_CONTROL,
                PRESS,
                modifiers=CONTROL,
            )
            state = terminal.desktop_state()
            self.assertEqual(state["icon"], 1)
            self.assertNotEqual(state["hovered_hyperlink"], 0)

            terminal.write(b"\x1b[1;1H\x1b]8;;\x1b\\X")
            self.assertEqual(
                (
                    terminal.desktop_state()["icon"],
                    terminal.desktop_state()["hovered_hyperlink"],
                ),
                (0, 0),
            )

            terminal.write(
                b"\x1b[1;1H"
                b"\x1b]8;id=new;https://new.test\x1b\\"
                b"Z"
                b"\x1b]8;;\x1b\\"
            )
            self.assertEqual(terminal.desktop_state()["icon"], 1)

            terminal.focus(False)
            self.assertEqual(terminal.desktop_state()["icon"], 0)
            terminal.focus(True)
            terminal.pointer(2, 2, modifiers=CONTROL)
            self.assertEqual(terminal.desktop_state()["icon"], 1)

            terminal.pointer_presence(False)
            self.assertEqual(terminal.desktop_state()["icon"], 0)
            terminal.pointer_presence(True)
            terminal.pointer(2, 2, modifiers=CONTROL)
            self.assertEqual(terminal.desktop_state()["icon"], 1)

            terminal.frontend_key_event(
                LEFT_CONTROL,
                RELEASE,
                modifiers=0,
            )
            self.assertEqual(terminal.desktop_state()["icon"], 0)

    def test_super_arms_the_hover_and_the_click_like_control(self):
        # Super is a hyperlink modifier everywhere Control is: on macOS
        # Command maps to it, and Command+click is the platform's link
        # convention.
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(
                b"\x1b[?25l"
                b"\x1b]8;id=one;https://one.test\x1b\\ab\x1b]8;;\x1b\\"
            )
            terminal.pointer(2, 2)
            self.assertEqual(terminal.desktop_state()["icon"], 0)

            terminal.frontend_key_event(LEFT_SUPER, PRESS, modifiers=SUPER)
            state = terminal.desktop_state()
            self.assertEqual(state["icon"], 1)
            self.assertNotEqual(state["hovered_hyperlink"], 0)
            terminal.frontend_key_event(LEFT_SUPER, RELEASE, modifiers=0)
            self.assertEqual(terminal.desktop_state()["icon"], 0)

            terminal.button(0, True, x=2, y=2, modifiers=SUPER)
            terminal.button(0, False, x=2, y=2, modifiers=SUPER)
            state = terminal.desktop_state()
            self.assertEqual(state["open_count"], 1)
            self.assertEqual(state["opened_uri"], b"https://one.test")

    def test_hover_marks_the_complete_current_occurrence(self):
        with Shitty(columns=6, rows=1) as terminal:
            terminal.write(
                b"\x1b[?25l"
                b"\x1b]8;id=left;https://left.test\x1b\\ab"
                b"\x1b]8;;\x1b\\ "
                b"\x1b]8;id=right;https://right.test\x1b\\cd"
                b"\x1b]8;;\x1b\\"
            )
            metrics = terminal.load_font("monospace", "")
            border = terminal.options()["border"]
            width, _, original = terminal.render_image("monospace", "")

            terminal.pointer(2, 2, modifiers=CONTROL)
            first_state = terminal.desktop_state()
            _, _, first = terminal.render_image("monospace", "")

            snapshot = terminal.snapshot()
            self.assertEqual(
                first_state["hovered_hyperlink"],
                snapshot.cell(0, 0).hyperlink,
            )
            for column in (0, 1):
                self.assertNotEqual(
                    cell_pixels(
                        original,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                    cell_pixels(
                        first,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                )
            for column in (3, 4):
                self.assertEqual(
                    cell_pixels(
                        original,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                    cell_pixels(
                        first,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                )

            terminal.pointer(5, 2, modifiers=CONTROL)
            second_state = terminal.desktop_state()
            _, _, second = terminal.render_image("monospace", "")
            self.assertEqual(
                second_state["hovered_hyperlink"],
                snapshot.cell(3, 0).hyperlink,
            )
            for column in (0, 1):
                self.assertEqual(
                    cell_pixels(
                        original,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                    cell_pixels(
                        second,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                )
            for column in (3, 4):
                self.assertNotEqual(
                    cell_pixels(
                        original,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                    cell_pixels(
                        second,
                        width,
                        border,
                        metrics["px"],
                        metrics["py"],
                        column,
                    ),
                )

    def test_hover_does_not_replace_existing_underline_style_or_color(self):
        with Shitty(columns=4, rows=1) as terminal:
            terminal.write(
                b"\x1b[?25l"
                b"\x1b[4:3;58:2::4:5:6m"
                b"\x1b]8;id=styled;https://styled.test\x1b\\ab"
                b"\x1b]8;;\x1b\\"
            )
            before = terminal.snapshot().cell(0, 0)
            _, _, original = terminal.render_image("monospace", "")

            terminal.pointer(2, 2, modifiers=CONTROL)
            after = terminal.snapshot().cell(0, 0)
            _, _, hovered = terminal.render_image("monospace", "")

            self.assertEqual(after.underline_style, before.underline_style)
            self.assertEqual(after.underline_color, before.underline_color)
            self.assertEqual(hovered, original)

    def test_control_click_is_captured_but_other_clicks_keep_reporting(self):
        with Shitty(columns=5, rows=1) as terminal:
            terminal.write(
                b"\x1b]8;id=one;https://one.test\x1b\\A"
                b"\x1b]8;;\x1b\\"
                b"\x1b]8;id=two;https://two.test\x1b\\B"
                b"\x1b]8;;\x1b\\ "
                b"\x1b[?1000h\x1b[?1006h"
            )

            terminal.button(0, True, x=2, y=2, modifiers=CONTROL)
            state = terminal.desktop_state()
            self.assertEqual(state["open_count"], 1)
            self.assertEqual(state["opened_uri"], b"https://one.test")
            terminal.button(0, False, x=3, y=2)
            self.assertEqual(terminal.desktop_state()["open_count"], 1)
            self.assertEqual(terminal.read_input(), b"")

            terminal.button(0, True, x=3, y=2)
            terminal.button(0, False, x=3, y=2)
            terminal.button(0, True, x=4, y=2, modifiers=CONTROL)
            terminal.button(0, False, x=4, y=2, modifiers=CONTROL)
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[<0;2;1M\x1b[<0;2;1m"
                b"\x1b[<16;3;1M\x1b[<16;3;1m",
            )
            self.assertEqual(terminal.desktop_state()["open_count"], 1)


if __name__ == "__main__":
    unittest.main()
