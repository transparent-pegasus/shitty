# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import unittest

from harness import Shitty


class TerminalModeTest(unittest.TestCase):
    def test_xterm_mode_40_gates_column_mode_changes(self):
        with Shitty(columns=80, rows=4) as terminal:
            terminal.write(b"\x1b[?40$p\x1b[?3h")
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[?40;2$y",
            )
            self.assertEqual(terminal.snapshot().columns, 80)

            terminal.write(b"\x1b[?40h\x1b[?3h")
            self.assertEqual(terminal.snapshot().columns, 132)

    def test_xterm_mode_41_wraps_pending_tab_before_tabulation(self):
        with Shitty(columns=16, rows=3) as terminal:
            terminal.write(b"\x1b[?41h" + b"x" * 16 + b"\tX")
            self.assertEqual(terminal.snapshot().lines[1], " " * 8 + "X" + " " * 7)

        with Shitty(columns=16, rows=3) as terminal:
            terminal.write(b"\x1b[?41l" + b"x" * 16 + b"\tX")
            self.assertEqual(terminal.snapshot().lines[1], "X" + " " * 15)

    def test_deccolm_resizes_and_clears_the_terminal_page(self):
        with Shitty(columns=80, rows=24) as terminal:
            terminal.write(b"\x1b[?40hcontent\x1b[?3h")
            snapshot = terminal.snapshot()
            self.assertEqual((snapshot.columns, snapshot.rows), (132, 24))
            self.assertEqual(snapshot.lines[0], " " * 132)
            self.assertEqual((snapshot.cursor_x, snapshot.cursor_y), (0, 0))

            terminal.write(b"content\x1b[?3l")
            snapshot = terminal.snapshot()
            self.assertEqual((snapshot.columns, snapshot.rows), (80, 24))
            self.assertEqual(snapshot.lines[0], " " * 80)
            self.assertEqual((snapshot.cursor_x, snapshot.cursor_y), (0, 0))

    def test_conformance_state_exposes_screen_and_mode_vector(self):
        with Shitty() as terminal:
            self.assertEqual(terminal.conformance_state(), {
                "screen": "Primary",
                "IRM": False,
                "SRM": True,
                "LNM": False,
                "DECCKM": False,
                "DECCOLM": False,
                "DECSCLM": False,
                "DECSCNM": False,
                "DECOM": False,
                "DECAWM": True,
                "DECARM": True,
                "DECTCEM": True,
                "DECNKM": False,
                "DECBKM": False,
                "DECLRMM": False,
            })

            terminal.write(
                b"\x1b[?40h"
                b"\x1b[4;20h"
                b"\x1b[?1;3;4;5;6;8;67;69h"
                b"\x1b="
                b"\x1b[?47h"
            )
            state = terminal.conformance_state()
            self.assertEqual(state["screen"], "Alternate")
            for mode in (
                "IRM", "LNM", "DECCKM", "DECCOLM", "DECSCNM", "DECOM",
                "DECNKM", "DECBKM", "DECLRMM",
            ):
                self.assertTrue(state[mode], mode)
            self.assertTrue(state["DECSCLM"])
            self.assertTrue(state["DECARM"])

    def test_decll_tracks_each_host_led_independently(self):
        with Shitty() as terminal:
            terminal.write(b"\x1b[1;2;3q\x1b[22q")
            self.assertEqual(terminal.protocol_state()[1], 0b101)
            terminal.write(b"\x1b[q")
            self.assertEqual(terminal.protocol_state()[1], 0)

    def test_decscreen_mode_reverses_the_composed_display(self):
        with Shitty() as terminal:
            terminal.write(b"\x1b[?5h")
            self.assertEqual(terminal.protocol_state()[0], 1)
            terminal.write(b"\x1b[?5l")
            self.assertEqual(terminal.protocol_state()[0], 0)

    def test_decnkm_controls_and_reports_numeric_keypad_mode(self):
        with Shitty() as terminal:
            terminal.write(b"\x1b[?66$p")
            terminal.key("KP_1")
            self.assertEqual(terminal.read_input(), b"\x1b[?66;2$y1")

            terminal.write(b"\x1b[?66h\x1b[?66$p")
            terminal.key("KP_1")
            self.assertEqual(
                terminal.read_input(), b"\x1b[?66;1$y\x1bOq"
            )

            terminal.write(b"\x1b[?66l\x1b[?66$p")
            terminal.key("KP_1")
            self.assertEqual(terminal.read_input(), b"\x1b[?66;2$y1")

    def test_decscl_selects_protocol_levels_and_reports_vt420_up(self):
        with Shitty() as terminal:
            terminal.write(b"\x1b[61;1\"p\x1b[4$p")
            self.assertEqual(terminal.read_input(), b"")

            terminal.write(b"\x1b[62;1\"p\x1b[4$p")
            self.assertEqual(terminal.read_input(), b"")

            terminal.write(b"\x1b[63;1\"p\x1b[4$p")
            self.assertEqual(terminal.read_input(), b"\x1b[4;2$y")

            for level in (64, 65):
                with self.subTest(level=level):
                    terminal.write(
                        f"\x1b[{level};1\"p\x1bP$q\"p\x1b\\".encode()
                    )
                    self.assertEqual(
                        terminal.read_input(),
                        f"\x1bP1$r{level};1\"p\x1b\\".encode(),
                    )

    def test_decscl_hard_resets_terminal_before_selecting_level(self):
        with Shitty() as terminal:
            terminal.write(
                b"text\x1b[6;7H\x1b7\x1b[4h"
                b"\x1b[61\"p\x1b8X"
            )
            snapshot = terminal.snapshot()
            self.assertEqual((snapshot.cursor_x, snapshot.cursor_y), (1, 0))
            self.assertEqual(snapshot.lines[0][:2], "X ")

    def test_decscl_gates_level_specific_sequences(self):
        with Shitty(columns=10, rows=4) as terminal:
            terminal.write(
                b"\x1b[63;1\"p\x1b[?69h\x1b[3;5s\x1b[1;5Habc"
            )
            self.assertEqual(terminal.snapshot().cursor_x, 7)

            terminal.write(
                b"\x1b[64;1\"p\x1b[?69h\x1b[3;5s\x1b[1;3Habc"
            )
            self.assertEqual(terminal.snapshot().cursor_x, 4)

    def test_decncsm_preserves_page_only_at_vt500_level(self):
        with Shitty(columns=80, rows=4) as terminal:
            terminal.write(
                b"\x1b[64;1\"p\x1b[?95h\x1b[?95$p"
                b"\x1b[?40hx\x1b[?3h"
            )
            self.assertEqual(terminal.read_input(), b"\x1b[?95;0$y")
            self.assertEqual(terminal.snapshot().lines[0][0], " ")

            terminal.write(
                b"\x1b[65;1\"p\x1b[?95h\x1b[?95$p"
                b"\x1b[?40hx\x1b[2;4r\x1b[?69h\x1b[3;20s\x1b[?3h"
            )
            self.assertEqual(terminal.read_input(), b"\x1b[?95;1$y")
            snapshot = terminal.snapshot()
            self.assertEqual(
                (snapshot.columns, snapshot.cursor_x, snapshot.cursor_y),
                (132, 0, 0),
            )
            self.assertEqual(snapshot.lines[0][0], "x")

    def test_dec_mode_availability_reports_by_level(self):
        with Shitty() as terminal:
            terminal.write(
                b"\x1b[63;1\"p"
                b"\x1b[?4$p\x1b[?8$p"
                b"\x1b[?60$p\x1b[?61$p\x1b[?64$p\x1b[?68$p\x1b[?73$p"
                b"\x1b[?81$p\x1b[?100$p"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[?4;2$y\x1b[?8;1$y"
                b"\x1b[?60;4$y\x1b[?61;4$y\x1b[?64;4$y"
                b"\x1b[?68;4$y\x1b[?73;4$y"
                b"\x1b[?81;0$y\x1b[?100;0$y",
            )

            terminal.write(b"\x1b[64;1\"p\x1b[?81$p\x1b[?100$p")
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[?81;4$y\x1b[?100;0$y",
            )

            terminal.write(
                b"\x1b[65;1\"p"
                b"\x1b[?34$p\x1b[?35$p\x1b[?36$p\x1b[?57$p"
                b"\x1b[?96$p\x1b[?97$p\x1b[?98$p\x1b[?99$p"
                b"\x1b[?100$p\x1b[?101$p\x1b[?102$p"
                b"\x1b[?103$p\x1b[?104$p\x1b[?106$p"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[?34;4$y\x1b[?35;4$y\x1b[?36;4$y"
                b"\x1b[?57;4$y\x1b[?96;4$y\x1b[?97;4$y"
                b"\x1b[?98;4$y\x1b[?99;4$y\x1b[?100;4$y"
                b"\x1b[?101;4$y\x1b[?102;4$y\x1b[?103;4$y"
                b"\x1b[?104;4$y\x1b[?106;4$y",
            )

    def test_meta_mode_sets_the_eighth_input_bit(self):
        with Shitty() as terminal:
            terminal.write(b"\x1b[?1034h")
            terminal.char("a", modifiers=4)
            self.assertEqual(terminal.read_input(), b"\xe1")
            terminal.write(b"\x1b[?1034l")
            terminal.char("a", modifiers=4)
            self.assertEqual(terminal.read_input(), b"\x1ba")

    def test_reverse_wrap_follows_soft_wrapped_lines_only(self):
        with Shitty(columns=4, rows=3) as terminal:
            terminal.write(b"abcdX\x1b[?45h\x1b[2;1H\bY")
            snapshot = terminal.snapshot()
            self.assertEqual(snapshot.cell(3, 0).char, "Y")

            terminal.write(b"\x1b[3;1H\bZ")
            self.assertEqual(terminal.snapshot().cell(0, 2).char, "Z")

    def test_reverse_wrap_backspace_cancels_pending_wrap_first(self):
        with Shitty(columns=8, rows=3) as terminal:
            terminal.write(b"\x1b[?7;45h\x1b[1;7Hab\b\x1b[6n")
            self.assertEqual(terminal.read_input(), b"\x1b[1;8R")

    def test_extended_reverse_wrap_crosses_hard_line_boundaries(self):
        with Shitty(columns=4, rows=3) as terminal:
            terminal.write(b"\x1b[?45h\x1b[?1045h\x1b[3;1H\x1b[2DY")
            snapshot = terminal.snapshot()
            self.assertEqual(snapshot.cell(2, 1).char, "Y")

    def test_alternate_scroll_mode_is_reported(self):
        # DECSET 1007 read back through the terminal's own state, which is
        # the accessor a client outside the terminal uses; it is
        # independent of the alternate screen it applies on.
        with Shitty() as terminal:
            self.assertEqual(terminal.protocol_state()[4], 0)
            terminal.write(b"\x1b[?1007h")
            self.assertEqual(terminal.protocol_state()[4], 1)
            terminal.write(b"\x1b[?1049h")
            self.assertEqual(terminal.protocol_state()[4], 1)
            terminal.write(b"\x1b[?1007l")
            self.assertEqual(terminal.protocol_state()[4], 0)


if __name__ == "__main__":
    unittest.main()

