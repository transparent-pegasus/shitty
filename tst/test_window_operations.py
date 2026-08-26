# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import unittest

from harness import Shitty


def window_terminal(**kwargs):
    return Shitty(
        columns=10,
        rows=4,
        extra_arguments=("-allowWindowOps", "true"),
        **kwargs,
    )


class WindowOperationsTest(unittest.TestCase):
    def test_window_operations_are_disabled_by_default(self):
        with Shitty(columns=10, rows=4) as terminal:
            terminal.write(b"\x9b2t\x1b[4;120;320t\x1b[11t")
            self.assertEqual(terminal.read_actions(), [])
            self.assertEqual(terminal.read_input(), b"")

    def test_window_commands_one_through_ten_reach_backend(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b[1t\x1b[2t\x1b[3;40;50t\x1b[4;120;320t"
                b"\x1b[5t\x1b[6t\x1b[7t\x1b[8;24;80t"
                b"\x1b[9;3t\x1b[10;2t"
            )
            self.assertEqual(
                terminal.read_actions(),
                [
                    "WINDOW 1 0 0",
                    "WINDOW 2 0 0",
                    "WINDOW 3 40 50",
                    "WINDOW 4 120 320",
                    "WINDOW 5 0 0",
                    "WINDOW 6 0 0",
                    "WINDOW 7 0 0",
                    "WINDOW 8 24 80",
                    "WINDOW 9 3 0",
                    "WINDOW 10 2 0",
                ],
            )

    def test_resize_distinguishes_omitted_and_zero_dimensions(self):
        with window_terminal(glyph_px=2, glyph_py=4) as terminal:
            terminal.window_info(screen_width=30, screen_height=20)
            terminal.write(b"\x1b[8;;12t\x1b[8;6;t\x1b[8;0;0t")
            self.assertEqual(
                terminal.read_actions(),
                [
                    "WINDOW 8 4 12",
                    "WINDOW 8 6 12",
                    "WINDOW 8 4 13",
                ],
            )
            snapshot = terminal.model_snapshot()
            self.assertEqual((snapshot.columns, snapshot.rows), (13, 4))

    def test_window_state_reports_normal_and_iconified(self):
        with window_terminal() as terminal:
            terminal.write(b"\x1b[11t")
            self.assertEqual(terminal.read_input(), b"\x1b[1t")

            terminal.window_info(iconified=True)
            terminal.write(b"\x1b[11t")
            self.assertEqual(terminal.read_input(), b"\x1b[2t")

    def test_control_host_applies_iconify_and_move_requests(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b[2t\x1b[11t"
                b"\x1b[1t\x1b[11t"
                b"\x1b[3;1;2t\x1b[13t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[2t\x1b[1t\x1b[3;1;2t",
            )

    def test_control_host_applies_and_restores_maximized_size(self):
        with window_terminal() as terminal:
            terminal.window_info(screen_width=30, screen_height=20)
            terminal.write(b"\x1b[9;1t")
            maximized = terminal.model_snapshot()
            self.assertEqual((maximized.columns, maximized.rows), (26, 16))

            terminal.write(b"\x1b[9;0t")
            restored = terminal.model_snapshot()
            self.assertEqual((restored.columns, restored.rows), (10, 4))

    def test_fullscreen_startup_takes_the_screen_and_wins_over_maximized(self):
        # -fullscreen applies on the startup path, so the window opens
        # at the headless screen size. With -maximized alongside,
        # fullscreen is the one request made: toggling fullscreen off
        # restores the original geometry, which a lingering maximized
        # request would have held at the screen size instead.
        with Shitty(
            columns=10,
            rows=4,
            extra_arguments=(
                "-fullscreen",
                "-maximized",
                "-allowWindowOps",
                "true",
            ),
        ) as terminal:
            terminal.write(b"\x1b[18t")
            self.assertEqual(terminal.read_input(), b"\x1b[8;1076;1916t")

            terminal.write(b"\x1b[10;2t")
            terminal.write(b"\x1b[18t")
            self.assertEqual(terminal.read_input(), b"\x1b[8;4;10t")

    def test_window_position_reports_signed_coordinates_as_unsigned(self):
        with window_terminal() as terminal:
            terminal.window_info(x=-10, y=-20)
            terminal.write(b"\x1b[13t\x1b[13;2t")
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[3;65526;65516t\x1b[3;65526;65516t",
            )

    def test_text_area_and_outer_window_pixel_reports_are_distinct(self):
        with window_terminal() as terminal:
            terminal.write(b"\x1b[14t\x1b[14;2t")
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[4;4;10t\x1b[4;8;14t",
            )

    def test_geometry_query_matrix(self):
        with window_terminal() as terminal:
            terminal.write(b"\x1b[15t\x1b[16t\x1b[18t\x1b[19t")
            self.assertEqual(
                terminal.read_input(),
                b"\x1b[5;1080;1920t"
                b"\x1b[6;1;1t"
                b"\x1b[8;4;10t"
                b"\x1b[9;1076;1916t",
            )

    def test_undefined_queries_twelve_and_seventeen_are_ignored(self):
        with window_terminal() as terminal:
            terminal.write(b"\x1b[12t\x1b[17t")
            self.assertEqual(terminal.read_input(), b"")
            self.assertEqual(terminal.read_actions(), [])

    def test_icon_and_window_titles_have_independent_reports(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]1;icon\x1b\\\x1b]2;window\x1b\\"
                b"\x1b[20t\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]Licon\x1b\\\x1b]lwindow\x1b\\",
            )

    def test_title_modes_independently_control_hex_set_and_query(self):
        cases = (
            (b"\x1b[>2;1T\x1b[>0;3t", b"6162", b"ab"),
            (b"\x1b[>0;3T\x1b[>2;1t", b"ab", b"6162"),
            (b"\x1b[>2;3T\x1b[>0;1t", b"6162", b"6162"),
            (b"\x1b[>0;1T\x1b[>2;3t", b"ab", b"ab"),
        )
        for modes, title, expected in cases:
            with self.subTest(modes=modes):
                with window_terminal() as terminal:
                    terminal.write(
                        modes
                        + b"\x1b]1;" + title + b"\x1b\\"
                        + b"\x1b]2;" + title + b"\x1b\\"
                        + b"\x1b[20t\x1b[21t"
                    )
                    self.assertEqual(
                        terminal.read_input(),
                        b"\x1b]L" + expected + b"\x1b\\"
                        b"\x1b]l" + expected + b"\x1b\\",
                    )

    def test_hex_title_input_is_atomic_and_stops_at_controls(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]2;old\x1b\\\x1b[>0t"
                b"\x1b]2;4G\x1b\\\x1b[21t"
                b"\x1b]2;410A42\x1b\\\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]lold\x1b\\\x1b]lA\x1b\\",
            )

    def test_hex_title_queries_use_uppercase_and_empty_modes_reset_defaults(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b[>1t\x1b]2;Az\x1b\\\x1b[21t"
                b"\x1b[>t\x1b]2;Az\x1b\\\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]l417A\x1b\\\x1b]lAz\x1b\\",
            )

    def test_title_stack_selectors_restore_only_selected_title(self):
        cases = (
            (0, b"old-icon", b"old-window"),
            (1, b"old-icon", b"new-window"),
            (2, b"new-icon", b"old-window"),
        )
        for selector, expected_icon, expected_window in cases:
            with self.subTest(selector=selector):
                with window_terminal() as terminal:
                    terminal.write(
                        b"\x1b]1;old-icon\x1b\\"
                        b"\x1b]2;old-window\x1b\\"
                        + f"\x1b[22;{selector}t".encode()
                        + b"\x1b]1;new-icon\x1b\\"
                        b"\x1b]2;new-window\x1b\\"
                        + f"\x1b[23;{selector}t".encode()
                        + b"\x1b[20t\x1b[21t"
                    )
                    self.assertEqual(
                        terminal.read_input(),
                        b"\x1b]L" + expected_icon + b"\x1b\\"
                        b"\x1b]l" + expected_window + b"\x1b\\",
                    )

    def test_default_title_selector_saves_and_restores_both(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]1;old-icon\x1b\\\x1b]2;old-window\x1b\\"
                b"\x1b[22t"
                b"\x1b]1;new-icon\x1b\\\x1b]2;new-window\x1b\\"
                b"\x1b[23t\x1b[20t\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]Lold-icon\x1b\\\x1b]lold-window\x1b\\",
            )

    def test_empty_titles_can_be_saved_and_restored(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]1;\x1b\\\x1b]2;\x1b\\"
                b"\x1b[22t"
                b"\x1b]1;icon\x1b\\\x1b]2;window\x1b\\"
                b"\x1b[23t\x1b[20t\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]L\x1b\\\x1b]l\x1b\\",
            )

    def test_restore_from_empty_stack_is_a_noop(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]1;icon\x1b\\\x1b]2;window\x1b\\"
            )
            terminal.read_actions()
            terminal.write(b"\x1b[23t\x1b[20t\x1b[21t")
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]Licon\x1b\\\x1b]lwindow\x1b\\",
            )
            self.assertEqual(terminal.read_actions(), [])

    def test_invalid_title_selectors_do_not_change_stack(self):
        for invalid_operation in (b"\x1b[22;3t", b"\x1b[23;3t"):
            with self.subTest(operation=invalid_operation):
                with window_terminal() as terminal:
                    terminal.write(
                        b"\x1b]1;first\x1b\\\x1b[22t"
                        b"\x1b]1;second\x1b\\"
                        + invalid_operation
                        + b"\x1b]1;third\x1b\\\x1b[23t\x1b[20t"
                    )
                    self.assertEqual(
                        terminal.read_input(), b"\x1b]Lfirst\x1b\\"
                    )

    def test_title_stack_is_lifo(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]2;one\x1b\\\x1b[22;2t"
                b"\x1b]2;two\x1b\\\x1b[22;2t"
                b"\x1b]2;three\x1b\\"
                b"\x1b[23;2t\x1b[21t"
                b"\x1b[23;2t\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]ltwo\x1b\\\x1b]lone\x1b\\",
            )

    def test_pop_both_completes_sparse_entry_from_lower_stack_levels(self):
        with window_terminal() as terminal:
            terminal.write(
                b"\x1b]1;old-icon\x1b\\\x1b]2;old-window\x1b\\"
                b"\x1b[22;1t\x1b[22;2t"
                b"\x1b]1;new-icon\x1b\\\x1b]2;new-window\x1b\\"
                b"\x1b[23;0t\x1b[20t\x1b[21t"
            )
            self.assertEqual(
                terminal.read_input(),
                b"\x1b]Lold-icon\x1b\\\x1b]lold-window\x1b\\",
            )

    def test_title_stack_discards_oldest_entry_after_ten_pushes(self):
        with window_terminal() as terminal:
            for number in range(12):
                terminal.write(
                    f"\x1b]2;title-{number}\x1b\\\x1b[22;2t".encode()
                )
            terminal.write(b"\x1b]2;current\x1b\\")
            terminal.read_actions()

            reported = []
            for _ in range(11):
                terminal.write(b"\x1b[23;2t\x1b[21t")
                reported.append(terminal.read_input())

            self.assertEqual(
                reported,
                [f"\x1b]ltitle-{number}\x1b\\".encode()
                 for number in range(11, 1, -1)]
                + [b"\x1b]ltitle-2\x1b\\"],
            )


if __name__ == "__main__":
    unittest.main()
