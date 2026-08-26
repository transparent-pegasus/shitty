# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""-debug: the diagnostic trace a reporter runs when we cannot
reproduce - issues 83 and 86 are fullscreen transitions on hardware we
do not have. The trace must carry the resize sequence and a content
checksum, so a lost-content report distinguishes a model that dropped
its cells from a renderer that stopped drawing them."""

import tempfile
import unittest
from pathlib import Path

from harness import Shitty


class DebugTraceTest(unittest.TestCase):
    def test_trace_records_resizes_with_content_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.log"
            with Shitty(
                columns=10,
                rows=4,
                extra_arguments=("-debug", str(trace)),
            ) as terminal:
                terminal.write(b"payload")
                before = terminal.snapshot().lines
                terminal.resize(20, 6)
                after = terminal.snapshot().lines
                self.assertEqual(after[0].rstrip(), before[0].rstrip())
            lines = trace.read_text().splitlines()

        self.assertTrue(lines[0].endswith("trace opened"), lines[:2])
        resized = [line for line in lines if " resized grid " in line]
        self.assertTrue(resized, lines)
        # The resize evidence: geometry and a content checksum.
        self.assertIn("grid 20x6", resized[-1])
        self.assertIn("sum=", resized[-1])

    def test_without_the_option_no_file_appears(self):
        with tempfile.TemporaryDirectory() as directory:
            trace = Path(directory) / "trace.log"
            with Shitty(columns=10, rows=4) as terminal:
                terminal.write(b"payload")
                terminal.resize(20, 6)
            self.assertFalse(trace.exists())


if __name__ == "__main__":
    unittest.main()
