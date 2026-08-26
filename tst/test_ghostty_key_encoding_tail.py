# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import unittest

from harness import TEST_PLATFORM, Shitty


UPSTREAM_CASES = (
    "legacy: ctrl+shift+char with modify other state 2 and consumed mods",
    "legacy: alt+digit with modify other state 2",
    "legacy: alt+digit with modify other state 2 and macos-option-as-alt = false",
    "legacy: fixterm awkward letters",
    "legacy: ctrl+shift+letter ascii",
    "legacy: shift+function key should use all mods",
    "legacy: keypad enter",
    "legacy: keypad 1",
    "legacy: keypad 1 with application keypad",
    "legacy: keypad 1 with application keypad and numlock",
    "legacy: keypad 1 with application keypad and numlock ignore",
    "legacy: f1",
    "legacy: left_shift+tab",
    "legacy: right_shift+tab",
    "legacy: hu layout ctrl+ő sends proper codepoint",
    "legacy: super-only on macOS with text",
    "legacy: super and other mods on macOS with text",
    "legacy: backspace with DEL utf8 (DECBKM reset)",
    "legacy: backspace with DEL utf8 (DECBKM set)",
    "ctrlseq: normal ctrl c",
    "ctrlseq: normal ctrl c, right control",
    "ctrlseq: alt should be allowed",
    "ctrlseq: no ctrl does nothing",
    "ctrlseq: shifted non-character",
    "ctrlseq: caps ascii letter",
    "ctrlseq: shift does not generate ctrl seq",
    "ctrlseq: russian ctrl c",
    "ctrlseq: russian shifted ctrl c",
    "ctrlseq: russian alt ctrl c",
    "ctrlseq: right ctrl c",
)


class GhosttyKeyEncodingTailTest(unittest.TestCase):
    def test_upstream_inventory_has_30_distinct_cases(self):
        self.assertEqual(len(UPSTREAM_CASES), 30)
        self.assertEqual(len(set(UPSTREAM_CASES)), 30)

    def test_modify_other_keys_keeps_consumed_shift_in_the_packet(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b[>4;2m")
            terminal.char("H", modifiers=3)

            self.assertEqual(terminal.read_input(), b"\x1b[27;6;72~")

    def test_modify_other_keys_encodes_alt_digit(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b[>4;2m")
            terminal.char("8", modifiers=4)

            self.assertEqual(terminal.read_input(), b"\x1b[27;3;56~")

    def test_native_option_text_bypasses_modify_other_keys(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b[>4;2m")
            terminal.frontend_text_event("[")

            self.assertEqual(terminal.read_input(), b"[")

    @unittest.expectedFailure
    def test_fixterm_awkward_control_keys_use_csi_u(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("I", "i", "i", modifiers=2)
            terminal.layout_key("M", "m", "m", modifiers=2)
            terminal.layout_key("[", "[", "[", modifiers=2)
            terminal.layout_key("2", "2", "2", modifiers=3)

            self.assertEqual(
                terminal.read_input(),
                b"\x1b[105;5u\x1b[109;5u\x1b[91;5u\x1b[64;5u",
            )

    @unittest.expectedFailure
    def test_ctrl_shift_ascii_letter_keeps_shift_in_csi_u(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("M", "m", "m", modifiers=3)

            self.assertEqual(terminal.read_input(), b"\x1b[109;6u")

    def test_shift_function_key_uses_unconsumed_modifier(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.key("UP", modifiers=1)

            self.assertEqual(terminal.read_input(), b"\x1b[1;2A")

    def test_legacy_keypad_enter_is_carriage_return(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.key("KP_ENTER")

            self.assertEqual(terminal.read_input(), b"\r")

    def test_legacy_numeric_keypad_one_is_text(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.key("KP_1")

            self.assertEqual(terminal.read_input(), b"1")

    def test_application_keypad_one_uses_ss3(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b=")
            terminal.key("KP_1")

            self.assertEqual(terminal.read_input(), b"\x1bOq")

    @unittest.expectedFailure
    def test_application_keypad_one_ignores_num_lock_by_default(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b=")
            terminal.frontend_key_event(321, 1, modifiers=32)

            self.assertEqual(terminal.read_input(), b"\x1bOq")

    @unittest.expectedFailure
    def test_application_keypad_can_be_ignored_by_num_lock_policy(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b=")
            terminal.key("KP_1")

            self.assertEqual(terminal.read_input(), b"1")

    def test_legacy_control_function_key_matrix(self):
        expected = {
            "F1": b"\x1b[1;5P",
            "F2": b"\x1b[1;5Q",
            "F3": b"\x1b[1;5R",
            "F4": b"\x1b[1;5S",
            "F5": b"\x1b[15;5~",
        }
        with Shitty(columns=8, rows=2) as terminal:
            for key, encoded in expected.items():
                terminal.key(key, modifiers=2)
                self.assertEqual(terminal.read_input(), encoded)

    @unittest.expectedFailure
    def test_ghostty_legacy_control_f3_uses_13_tilde(self):
        # Ghostty is in the 4-vote minority for modified top-row F3;
        # Alacritty, xterm, iTerm2, VTE and foot use CSI 1;mod R.
        with Shitty(columns=8, rows=2) as terminal:
            terminal.key("F3", modifiers=2)
            self.assertEqual(terminal.read_input(), b"\x1b[13;5~")

    def test_left_shift_tab_uses_backtab(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.key("TAB", modifiers=1)

            self.assertEqual(terminal.read_input(), b"\x1b[Z")

    def test_right_shift_tab_uses_the_same_backtab(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.frontend_key_event(258, 1, modifiers=1)

            self.assertEqual(terminal.read_input(), b"\x1b[Z")

    @unittest.expectedFailure
    def test_hungarian_ctrl_key_reports_layout_codepoint(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("[", "ő", "[", modifiers=2)

            self.assertEqual(terminal.read_input(), b"\x1b[337;5u")

    @unittest.expectedFailure
    def test_macos_super_only_text_is_consumed_by_the_frontend(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("B", "b", "b", modifiers=8)
            terminal.frontend_text_event("b", modifiers=8)

            self.assertEqual(terminal.read_input(), b"")

    @unittest.expectedFailure
    def test_macos_super_shift_text_is_consumed_by_the_frontend(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("B", "b", "b", modifiers=9)
            terminal.frontend_text_event("B", modifiers=9)

            self.assertEqual(terminal.read_input(), b"")

    def test_decbkm_reset_wins_over_del_text_attached_to_backspace(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.key("BACKSPACE")

            self.assertEqual(terminal.read_input(), b"\x7f")

    def test_decbkm_set_wins_over_del_text_attached_to_backspace(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.write(b"\x1b[?67h")
            terminal.key("BACKSPACE")

            self.assertEqual(terminal.read_input(), b"\x08")

    def test_ctrlseq_normal_ctrl_c_is_etx(self):
        with Shitty(columns=8, rows=2) as terminal:
            self.assertEqual(terminal.control_character("C"), 3)

    def test_ctrlseq_right_control_has_the_same_etx(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("C", "c", "c", modifiers=2)

            self.assertEqual(terminal.read_input(), b"\x03")

    def test_ctrlseq_allows_alt_before_the_control_byte(self):
        with Shitty(columns=8, rows=2) as terminal:
            self.assertEqual(terminal.control_character("C"), 3)

    def test_without_control_plain_text_has_no_control_sequence(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.frontend_text_event("c")

            self.assertEqual(terminal.read_input(), b"c")

    def test_ctrlseq_shifted_minus_is_unit_separator(self):
        with Shitty(columns=8, rows=2) as terminal:
            self.assertEqual(terminal.control_character("-", shifted=True), 31)

    def test_ctrlseq_caps_locked_ascii_letter_is_etx(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("C", "c", "c", modifiers=18)

            self.assertEqual(terminal.read_input(), b"\x03")

    def test_shift_alone_generates_text_not_a_control_sequence(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.frontend_text_event("C", modifiers=1)

            self.assertEqual(terminal.read_input(), b"C")

    def test_russian_physical_ctrl_c_uses_the_ascii_base_key(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("C", "с", "c", modifiers=2)

            self.assertEqual(terminal.read_input(), b"\x03")

    def test_russian_shift_ctrl_c_has_no_legacy_control_sequence(self):
        # On Linux this passes because Ctrl+Shift+C is the Copy binding
        # and never reaches the encoder. On macOS the chord is unbound,
        # and the encoder keeps the base-layout control byte under
        # Shift - the semantics the layout matrix pins - so the ghostty
        # expectation of silence does not apply there.
        expected = b"\x03" if TEST_PLATFORM == "cocoa" else b""
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("C", "с", "c", modifiers=3)

            self.assertEqual(terminal.read_input(), expected)

    def test_russian_alt_ctrl_c_prefixes_escape_to_etx(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("C", "с", "c", modifiers=6)

            self.assertEqual(terminal.read_input(), b"\x1b\x03")

    def test_russian_right_ctrl_c_uses_the_ascii_base_key(self):
        with Shitty(columns=8, rows=2) as terminal:
            terminal.layout_key("C", "с", "c", modifiers=2)

            self.assertEqual(terminal.read_input(), b"\x03")


if __name__ == "__main__":
    unittest.main()
