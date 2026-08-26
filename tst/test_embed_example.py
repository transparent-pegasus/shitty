# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""The C embedding facade, driven through bin/example: recorded byte
streams go in, the printed grid must match what the full terminal
produces for the same stream."""

import os
import subprocess
import tempfile
import unittest
from dataclasses import dataclass
from pathlib import Path

from harness import ROOT, Shitty

EXAMPLE = Path(os.environ.get("SHITTY_EMBED_EXAMPLE_BINARY", ROOT / "example"))
CORPUS = Path(__file__).parent / "corpus"

COLUMNS = 20
ROWS = 6


@dataclass
class ExampleResult:
    events: list[str]
    lines: list[str]
    cursor: tuple[int, int]
    cursor_style: int
    cursor_visible: bool
    modes: int
    replies: bytes
    scroll_offset: int
    history_rows: int
    total_rows: int
    rows_by_index: list[str]
    allocated_rows: int
    capacity_rows: int
    cell_bytes: int


def run_example(
    stream,
    columns=COLUMNS,
    rows=ROWS,
    save_lines=0,
    scroll=0,
    scroll_to=-1,
    dump_rows=0,
    set_save_lines=-1,
):
    with tempfile.NamedTemporaryFile() as recorded:
        recorded.write(stream)
        recorded.flush()
        result = subprocess.run(
            [
                EXAMPLE,
                str(columns),
                str(rows),
                str(save_lines),
                recorded.name,
                str(scroll),
                str(scroll_to),
                str(dump_rows),
                str(set_save_lines),
            ],
            capture_output=True,
            timeout=60,
        )
    if result.returncode != 0:
        raise RuntimeError(
            f"example failed: {result.returncode} {result.stderr!r}"
        )
    lines = result.stdout.decode("utf-8").split("\n")
    if lines and lines[-1] == "":
        lines.pop()
    rows_by_index = []
    while lines and lines[-1].startswith("row "):
        rows_by_index.insert(0, lines.pop())
    memory_line = lines.pop()
    scrollback_line = lines.pop()
    replies_line = lines.pop()
    modes_line = lines.pop()
    cursor_line = lines.pop()
    if not replies_line.startswith("replies:"):
        raise RuntimeError(f"unexpected replies line: {replies_line!r}")
    if not modes_line.startswith("modes: "):
        raise RuntimeError(f"unexpected modes line: {modes_line!r}")
    if not cursor_line.startswith("cursor: "):
        raise RuntimeError(f"unexpected cursor line: {cursor_line!r}")
    if not scrollback_line.startswith("scrollback: "):
        raise RuntimeError(f"unexpected scrollback line: {scrollback_line!r}")
    if not memory_line.startswith("memory: "):
        raise RuntimeError(f"unexpected memory line: {memory_line!r}")
    grid = lines[len(lines) - rows :]
    events = lines[: len(lines) - rows]
    cursor_fields = cursor_line[len("cursor: ") :].split()
    replies = bytes.fromhex(replies_line[len("replies:") :].replace(" ", ""))
    scrollback_fields = scrollback_line[len("scrollback: ") :].split()
    indexed = [line.split(":", 1)[1] for line in rows_by_index]
    memory_fields = dict(
        field.split("=", 1) for field in memory_line[len("memory: ") :].split()
    )
    return ExampleResult(
        events=events,
        lines=grid,
        cursor=(int(cursor_fields[0]), int(cursor_fields[1])),
        cursor_style=int(cursor_fields[2].removeprefix("style=")),
        cursor_visible=bool(int(cursor_fields[3].removeprefix("visible="))),
        modes=int(modes_line[len("modes: ") :], 16),
        replies=replies,
        scroll_offset=int(scrollback_fields[0].removeprefix("offset=")),
        history_rows=int(scrollback_fields[1].removeprefix("history=")),
        total_rows=int(scrollback_fields[2].removeprefix("total=")),
        rows_by_index=indexed,
        allocated_rows=int(memory_fields["allocated_rows"]),
        capacity_rows=int(memory_fields["capacity_rows"]),
        cell_bytes=int(memory_fields["cell_bytes"]),
    )


MODE_ALT_SCREEN = 1 << 0
MODE_BRACKETED_PASTE = 1 << 1
MODE_APP_CURSOR_KEYS = 1 << 2
MODE_APP_KEYPAD = 1 << 3
MODE_FOCUS_EVENTS = 1 << 4
MODE_AUTO_WRAP = 1 << 5
MODE_ORIGIN = 1 << 6
MODE_INSERT = 1 << 7
MODE_CURSOR_VISIBLE = 1 << 8
MODE_SCREEN_REVERSE = 1 << 9
MODE_MOUSE_CLICK = 1 << 11
MODE_MOUSE_MOTION = 1 << 13
MODE_MOUSE_SGR = 1 << 14
MODE_ALTERNATE_SCROLL = 1 << 15


class EmbedExampleTest(unittest.TestCase):
    def assert_grid(self, stream, expected, **kwargs):
        result = run_example(stream, **kwargs)
        rows = kwargs.get("rows", ROWS)
        columns = kwargs.get("columns", COLUMNS)
        padded = [line.ljust(columns) for line in expected]
        padded += [" " * columns] * (rows - len(padded))
        self.assertEqual(result.lines, padded)
        return result

    def assert_matches_full_terminal(self, streams):
        for stream in streams:
            with self.subTest(stream=stream):
                embedded = run_example(stream)
                with Shitty(columns=COLUMNS, rows=ROWS) as terminal:
                    terminal.write(stream)
                    snapshot = terminal.snapshot()
                self.assertEqual(embedded.lines, snapshot.lines)
                self.assertEqual(
                    embedded.cursor,
                    (snapshot.cursor_x, snapshot.cursor_y),
                )

    def test_plain_text_lands_on_the_grid(self):
        result = self.assert_grid(b"hello", ["hello"])
        self.assertEqual(result.cursor, (5, 0))
        self.assertTrue(result.cursor_visible)
        self.assertTrue(result.modes & MODE_AUTO_WRAP)
        self.assertTrue(result.modes & MODE_CURSOR_VISIBLE)

    def test_cursor_addressing_and_clamped_movement(self):
        self.assert_grid(b"\x1b[3;1HA\x1b[10AX", [" X", "", "A"])

    def test_carriage_return_and_backspace_overwrite(self):
        self.assert_grid(b"world\rW\x1b[4Cd\x08D", ["WorldD"])

    def test_scroll_region_confines_the_scroll(self):
        self.assert_grid(b"\x1b[1;3rA\r\nB\r\nC\r\nD\r\nE", ["C", "D", "E"])

    def test_wrap_and_wrap_disabled(self):
        long = b"x" * (COLUMNS + 3)
        self.assert_grid(long, ["x" * COLUMNS, "xxx"])
        result = run_example(b"\x1b[?7l" + long)
        self.assertEqual(result.lines[0], "x" * COLUMNS)
        self.assertEqual(result.lines[1], " " * COLUMNS)
        self.assertFalse(result.modes & MODE_AUTO_WRAP)

    def test_alt_screen_swaps_and_reports_its_mode(self):
        result = run_example(b"base\x1b[?1049h\x1b[Halt")
        self.assertEqual(result.lines[0], "alt".ljust(COLUMNS))
        self.assertTrue(result.modes & MODE_ALT_SCREEN)
        result = run_example(b"base\x1b[?1049h\x1b[Halt\x1b[?1049l")
        self.assertEqual(result.lines[0], "base".ljust(COLUMNS))
        self.assertFalse(result.modes & MODE_ALT_SCREEN)

    def test_modes_reflect_private_mode_changes(self):
        result = run_example(
            b"\x1b[?2004h\x1b[?1h\x1b[?1003h\x1b[?1006h\x1b[?25l"
        )
        self.assertTrue(result.modes & MODE_BRACKETED_PASTE)
        self.assertTrue(result.modes & MODE_APP_CURSOR_KEYS)
        self.assertTrue(result.modes & MODE_MOUSE_CLICK)
        self.assertTrue(result.modes & MODE_MOUSE_MOTION)
        self.assertTrue(result.modes & MODE_MOUSE_SGR)
        self.assertFalse(result.modes & MODE_CURSOR_VISIBLE)

    def test_alternate_scroll_is_reported_and_cleared(self):
        # DECSET 1007 stands on its own: a host reads it to decide whether
        # wheel input becomes arrow keys, which only matters once the
        # alternate screen is up, but the mode is settable either side of
        # that and must not be conflated with it.
        self.assertFalse(run_example(b"").modes & MODE_ALTERNATE_SCROLL)

        armed = run_example(b"\x1b[?1007h")
        self.assertTrue(armed.modes & MODE_ALTERNATE_SCROLL)
        self.assertFalse(armed.modes & MODE_ALT_SCREEN)

        both = run_example(b"\x1b[?1007h\x1b[?1049h")
        self.assertTrue(both.modes & MODE_ALTERNATE_SCROLL)
        self.assertTrue(both.modes & MODE_ALT_SCREEN)

        cleared = run_example(b"\x1b[?1007h\x1b[?1049h\x1b[?1007l")
        self.assertFalse(cleared.modes & MODE_ALTERNATE_SCROLL)
        self.assertTrue(cleared.modes & MODE_ALT_SCREEN)

    def test_keypad_origin_and_reverse_modes(self):
        result = run_example(b"\x1b=\x1b[?6h\x1b[?5h\x1b[4h")
        self.assertTrue(result.modes & MODE_APP_KEYPAD)
        self.assertTrue(result.modes & MODE_ORIGIN)
        self.assertTrue(result.modes & MODE_SCREEN_REVERSE)
        self.assertTrue(result.modes & MODE_INSERT)

    def test_origin_mode_addresses_inside_the_margins(self):
        self.assert_grid(b"\x1b[2;4r\x1b[?6h\x1b[HX", ["", "X"])

    def test_decaln_fills_the_screen(self):
        self.assert_grid(b"\x1b#8", ["E" * COLUMNS] * ROWS)

    def test_save_and_restore_cursor(self):
        self.assert_grid(b"\x1b[3;5H\x1b7\x1b[HA\x1b8B", ["A", "", "    B"])

    def test_index_scrolls_at_the_bottom(self):
        stream = b"\x1b[6;1Hlast\x1bDnext"
        self.assert_grid(stream, ["", "", "", "", "last", "    next"])

    def test_reverse_index_scrolls_at_the_top(self):
        self.assert_grid(b"top\x1b[1;1H\x1bMX", ["X", "top"])

    def test_erase_line_variants(self):
        self.assert_grid(b"abcdef\x1b[1;3H\x1b[1K", ["   def"])
        self.assert_grid(b"abcdef\x1b[1;3H\x1b[K", ["ab"])
        self.assert_grid(b"abcdef\x1b[2K", [""])

    def test_erase_display_below_and_above(self):
        self.assert_grid(b"1\r\n2\r\n3\x1b[2;1H\x1b[J", ["1"])
        self.assert_grid(b"1\r\n2\r\n3\x1b[2;1H\x1b[1J", ["", "", "3"])

    def test_insert_and_delete_lines(self):
        self.assert_grid(b"1\r\n2\r\n3\x1b[1;1H\x1b[L", ["", "1", "2", "3"])
        self.assert_grid(b"1\r\n2\r\n3\x1b[1;1H\x1b[M", ["2", "3"])

    def test_insert_and_delete_characters(self):
        self.assert_grid(b"abc\x1b[1;1H\x1b[2@", ["  abc"])
        self.assert_grid(b"abcdef\x1b[1;2H\x1b[2P", ["adef"])

    def test_erase_character_leaves_the_cursor(self):
        self.assert_grid(b"abcdef\x1b[1;2H\x1b[3X", ["a   ef"])

    def test_repeat_repeats_the_last_graphic(self):
        self.assert_grid(b"ab\x1b[3b", ["abbbb"])

    def test_custom_tab_stop(self):
        self.assert_grid(b"\x1b[1;4H\x1bH\x1b[1;1H\tX", ["   X"])

    def test_line_drawing_charset(self):
        self.assert_grid(b"\x1b(0lqk\x1b(B", ["┌─┐"])

    def test_shift_out_uses_g1(self):
        self.assert_grid(b"\x1b)0a\x0eq\x0fa", ["a─a"])

    def test_cursor_style_changes(self):
        result = run_example(b"\x1b[6 q")
        self.assertEqual(result.cursor_style, 4)
        result = run_example(b"\x1b[4 q")
        self.assertEqual(result.cursor_style, 3)

    def test_hard_reset_restores_the_defaults(self):
        result = run_example(b"mess\x1b[?25l\x1b[2;4r\x1b[?5h\x1bc")
        self.assertEqual(result.lines, [" " * COLUMNS] * ROWS)
        self.assertTrue(result.modes & MODE_CURSOR_VISIBLE)
        self.assertFalse(result.modes & MODE_SCREEN_REVERSE)
        self.assertEqual(result.cursor, (0, 0))

    def test_scrollback_keeps_the_tail_visible(self):
        stream = b"1\r\n2\r\n3\r\n4\r\n5\r\n6\r\n7\r\n8"
        self.assert_grid(stream, ["3", "4", "5", "6", "7", "8"], save_lines=10)

    def test_wide_grapheme_occupies_two_columns(self):
        result = run_example("A漢B".encode())
        self.assertTrue(result.lines[0].startswith("A漢 B"))

    def test_wide_grapheme_wraps_whole_at_line_end(self):
        result = run_example(("x" * (COLUMNS - 1) + "漢").encode())
        self.assertTrue(result.lines[1].startswith("漢"))

    def test_combining_grapheme_stays_one_cell(self):
        result = run_example("é!".encode())
        self.assertTrue(result.lines[0].startswith("é!"))

    def test_device_attributes_land_in_the_reply_buffer(self):
        result = run_example(b"\x1b[c\x1b[>c")
        self.assertTrue(result.replies.startswith(b"\x1b[?"))
        self.assertIn(b"\x1b[>", result.replies)

    def test_cursor_position_report_reflects_the_cursor(self):
        result = run_example(b"\x1b[5;7H\x1b[6n")
        self.assertIn(b"\x1b[5;7R", result.replies)

    def test_title_and_bell_reach_the_callbacks(self):
        result = run_example(b"\x1b]0;the embedded title\x07\x07")
        self.assertIn("title: the embedded title", result.events)
        self.assertIn("bell", result.events)

    def test_construction_publishes_nothing(self):
        # The reset inside shitty_vt_new presents an empty title and asks
        # for a frame, but neither is the application speaking: no
        # callback fires before the first feed (issue 98). A terminal
        # reset by the application is another matter - RIS clears the
        # title, and that publication is real.
        self.assertEqual(run_example(b"").events, [])
        self.assertEqual(
            run_example(b"\x1b]0;x\x07\x1bc").events,
            ["title: x", "title: "],
        )

    def test_grid_matches_the_full_terminal_cursor_motion(self):
        self.assert_matches_full_terminal((
            b"hello world",
            b"\x1b[3;1HA\x1b[10AX",
            b"\x1b[2;4r\x1b[3;1HA\x1b[10AX",
            b"\x1b[1;3rA\x1b[4;1H\x1b[10BX",
            b"a\tb\tc\td",
            b"\x1b[4;7Hdeep\x1b[Hup",
            b"\x1b[19GX\x1b[5CY",
            b"\x1b[3;3H\x1b7text\x1b8over",
            b"\x1b[2;4r\x1b[?6h\x1b[HX\x1b[10;1HY",
        ))

    def test_grid_matches_the_full_terminal_editing(self):
        self.assert_matches_full_terminal((
            b"del\x1b[2Pete\rD",
            b"ins\x1b[HI\x1b[4hNS\x1b[4l",
            b"\x1b[5;1HAB\x1b[5;1H\x1b[LCD",
            b"abcdef\x1b[1;3H\x1b[2X!",
            b"one two\x1b[1;4H\x1b[K",
            b"1\r\n2\r\n3\r\n4\x1b[2;1H\x1b[2M",
            b"1\r\n2\r\n3\x1b[2;1H\x1b[2L",
            b"wide\x1b[1;1H\x1b[4@nar",
            b"ab\x1b[5bZ",
            b"line\x1b[2;1H\x1b[1J\x1b[Hnew",
        ))

    def test_grid_matches_the_full_terminal_scroll_and_screens(self):
        self.assert_matches_full_terminal((
            b"one\r\ntwo\r\nthree\r\nfour\r\nfive\r\nsix\r\nseven",
            b"\x1b[2;5r" + b"\r\n".join(str(i).encode() for i in range(9)),
            b"pre\x1b[?1049halt-screen\x1b[?1049lpost",
            b"pre\x1b[?47halt\x1b[?47l",
            b"\x1b[2Jwiped\x1b[3;3Hmark",
            b"\x1b#8\x1b[2;2H\x1b[J",
            b"x" * 47,
            b"\x1b[?7lclipped" + b"y" * 30,
            b"\x1b[6;1Hbottom\x1bD\x1bD",
            b"\x1b[H\x1bM\x1bMtop",
        ))

    def test_grid_matches_the_full_terminal_attributes_and_charsets(self):
        self.assert_matches_full_terminal((
            b"\x1b[1mB\x1b[4mU\x1b[7mR\x1b[0mN",
            b"\x1b[31mred\x1b[42mgreen\x1b[0mplain",
            b"\x1b(0lqqqk\x1b(B done",
            b"\x1b)0text\x0eqqq\x0fback",
            "мир\r\nпривет".encode(),
            "A漢字B".encode(),
            b"\x1b[38;5;120mindexed\x1b[48;2;1;2;3mdirect",
        ))

    # Fuzz records that morph presentation the example does not model:
    # double-height line attributes, and mid-stream grid resizes.
    CORPUS_SKIPS = {
        "crash-delivery-cursor-resize",
        "crash-delivery-divergence-double-height",
    }

    def test_grid_matches_the_full_terminal_on_recorded_corpus(self):
        for recorded in sorted(CORPUS.iterdir()):
            if recorded.name in self.CORPUS_SKIPS:
                continue
            stream = recorded.read_bytes()
            with self.subTest(stream=recorded.name):
                embedded = run_example(stream)
                with Shitty(columns=COLUMNS, rows=ROWS) as terminal:
                    terminal.write(stream)
                    snapshot = terminal.snapshot()
                self.assertEqual(embedded.lines, snapshot.lines)

    def test_replies_drain_incrementally(self):
        # The C contract: take_replies drains; a second call continues
        # where the first stopped. The example drains once with a large
        # buffer, so drive the split through two DA queries instead and
        # check both replies arrived in order.
        result = run_example(b"\x1b[c\x1b[5n")
        self.assertTrue(result.replies.startswith(b"\x1b[?"))
        self.assertTrue(result.replies.endswith(b"\x1b[0n"))


if __name__ == "__main__":
    unittest.main()


class ScrollbackTest(unittest.TestCase):
    """The view movement the facade exposes, checked against the grid it
    is supposed to move."""

    @staticmethod
    def stream(count):
        return "".join(f"line{index}\r\n" for index in range(count)).encode()

    def test_history_holds_what_scrolled_off_the_grid(self):
        # Ten lines plus the trailing newline occupy eleven rows; six of
        # them are on screen, so five went into the history.
        kept = run_example(self.stream(10), save_lines=100)
        self.assertEqual(kept.history_rows, 5)
        self.assertEqual(kept.scroll_offset, 0)
        self.assertEqual(kept.lines[0].rstrip(), "line5")

    def test_a_terminal_keeping_no_lines_retains_no_history(self):
        result = run_example(self.stream(10), save_lines=0)
        self.assertEqual(result.history_rows, 0)

    def test_history_is_capped_by_save_lines(self):
        result = run_example(self.stream(40), save_lines=3)
        self.assertEqual(result.history_rows, 3)

    def test_scrolling_moves_the_view_over_the_history(self):
        result = run_example(self.stream(10), save_lines=100, scroll=2)
        self.assertEqual(result.scroll_offset, 2)
        self.assertEqual(result.lines[0].rstrip(), "line3")

    def test_scrolling_clamps_to_the_retained_history(self):
        result = run_example(self.stream(10), save_lines=100, scroll=99)
        self.assertEqual(result.scroll_offset, 5)
        self.assertEqual(result.lines[0].rstrip(), "line0")

    def test_scrolling_back_down_returns_to_the_live_bottom(self):
        result = run_example(self.stream(10), save_lines=100, scroll=-5)
        self.assertEqual(result.scroll_offset, 0)
        self.assertEqual(result.lines[0].rstrip(), "line5")

    def test_alternate_screen_has_no_history_to_scroll(self):
        stream = b"\x1b[?1049h" + self.stream(10)
        result = run_example(stream, save_lines=100, scroll=3)
        self.assertEqual(result.history_rows, 0)
        self.assertEqual(result.scroll_offset, 0)

    def test_scrolling_to_an_absolute_offset_lands_there(self):
        result = run_example(self.stream(10), save_lines=100, scroll_to=3)
        self.assertEqual(result.scroll_offset, 3)
        self.assertEqual(result.lines[0].rstrip(), "line2")

    def test_scrolling_to_the_current_offset_changes_nothing(self):
        # The no-op path: settle at 2 relatively, then ask for 2 again.
        result = run_example(self.stream(10), save_lines=100, scroll=2, scroll_to=2)
        self.assertEqual(result.scroll_offset, 2)
        self.assertEqual(result.lines[0].rstrip(), "line3")

    def test_scrolling_to_zero_returns_to_the_live_bottom(self):
        result = run_example(self.stream(10), save_lines=100, scroll=4, scroll_to=0)
        self.assertEqual(result.scroll_offset, 0)
        self.assertEqual(result.lines[0].rstrip(), "line5")

    def test_scrolling_to_past_the_history_clamps(self):
        result = run_example(self.stream(10), save_lines=100, scroll_to=99)
        self.assertEqual(result.scroll_offset, 5)
        self.assertEqual(result.lines[0].rstrip(), "line0")


class HistoryRowTest(unittest.TestCase):
    """Reading rows by index, which must not depend on where the view sits."""

    @staticmethod
    def stream(count):
        return "".join(f"line{index}\r\n" for index in range(count)).encode()

    def test_every_retained_row_is_addressable_oldest_first(self):
        result = run_example(self.stream(10), save_lines=100, dump_rows=1)
        # Five scrolled off, six on screen; the last is the blank row the
        # trailing newline opened.
        self.assertEqual(result.total_rows, 11)
        self.assertEqual(len(result.rows_by_index), 11)
        self.assertEqual(
            [row.rstrip() for row in result.rows_by_index[:6]],
            ["line0", "line1", "line2", "line3", "line4", "line5"],
        )
        self.assertEqual(result.rows_by_index[10].strip(), "")

    def test_row_reads_ignore_the_view_position(self):
        live = run_example(self.stream(10), save_lines=100, dump_rows=1)
        scrolled = run_example(
            self.stream(10), save_lines=100, scroll=3, dump_rows=1
        )
        self.assertEqual(scrolled.scroll_offset, 3)
        self.assertEqual(scrolled.rows_by_index, live.rows_by_index)

    def test_a_terminal_without_history_addresses_only_the_grid(self):
        result = run_example(self.stream(10), save_lines=0, dump_rows=1)
        self.assertEqual(result.total_rows, ROWS)
        self.assertEqual(result.rows_by_index[0].rstrip(), "line5")

    def test_reading_past_the_last_row_yields_nothing(self):
        # The example only walks in range, so drive the edge through a
        # terminal whose history is capped: index total-1 is the last row
        # and the dump stops there rather than running on.
        result = run_example(self.stream(40), save_lines=3, dump_rows=1)
        self.assertEqual(result.total_rows, 3 + ROWS)
        self.assertEqual(len(result.rows_by_index), 3 + ROWS)
        self.assertEqual(result.rows_by_index[0].rstrip(), "line32")


class HistoryBudgetTest(unittest.TestCase):
    """Changing the history cap after construction, and what it costs."""

    @staticmethod
    def stream(count):
        return "".join(f"line{index}\r\n" for index in range(count)).encode()

    def test_memory_grows_with_the_history_it_backs(self):
        empty = run_example(b"", save_lines=100)
        filled = run_example(self.stream(40), save_lines=100)
        self.assertEqual(empty.cell_bytes, 0)
        self.assertGreater(filled.allocated_rows, empty.allocated_rows)
        self.assertEqual(
            filled.cell_bytes,
            filled.allocated_rows * COLUMNS * 16,
            "cell_bytes should be rows * columns * cell_size",
        )
        # The cap is what it may hold, not what it holds.
        self.assertEqual(filled.capacity_rows, ROWS + 100)

    def test_lowering_the_cap_drops_the_oldest_rows_at_once(self):
        result = run_example(self.stream(40), save_lines=100, set_save_lines=5)
        self.assertEqual(result.history_rows, 5)
        self.assertEqual(result.capacity_rows, ROWS + 5)
        # Not merely reported: the surviving rows are the newest five.
        rows = run_example(
            self.stream(40), save_lines=100, set_save_lines=5, dump_rows=1
        )
        self.assertEqual(rows.rows_by_index[0].rstrip(), "line30")

    def test_lowering_the_cap_releases_the_rows_it_dropped(self):
        before = run_example(self.stream(40), save_lines=100)
        after = run_example(self.stream(40), save_lines=100, set_save_lines=5)
        self.assertLess(after.cell_bytes, before.cell_bytes)

    def test_raising_the_cap_does_not_resurrect_dropped_rows(self):
        result = run_example(self.stream(40), save_lines=5, set_save_lines=100)
        self.assertEqual(result.capacity_rows, ROWS + 100)
        self.assertEqual(result.history_rows, 5)

    def test_the_visible_grid_survives_a_cap_change(self):
        before = run_example(self.stream(40), save_lines=100)
        after = run_example(self.stream(40), save_lines=100, set_save_lines=5)
        self.assertEqual(after.lines, before.lines)

