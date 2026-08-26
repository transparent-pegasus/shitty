# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

import importlib.util
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "lib" / "vterm" / "unicode_data.py"
UNICODE_ROOT = ROOT / "ext" / "unicode"


def load_generator():
    spec = importlib.util.spec_from_file_location("unicode_data_generator", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def property_at(database, codepoint):
    page = database["page_indices"][codepoint // 0x100]
    property_index = database["pages"][page][codepoint % 0x100]
    return database["properties"][property_index]


class UnicodeDataGeneratorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.generator = load_generator()
        cls.database = cls.generator.build_database(UNICODE_ROOT)
        cls.header = cls.generator.emit_header(cls.database)

    def test_generated_table_is_compact_and_complete(self):
        self.assertEqual(len(self.database["page_indices"]), 0x1100)
        self.assertEqual(len(self.database["pages"]), 175)
        self.assertEqual(len(self.database["properties"]), 83)
        self.assertLess(max(self.database["page_indices"]), 0x100)
        self.assertLess(
            max(value for page in self.database["pages"] for value in page),
            0x100,
        )
        self.assertEqual(len(self.database["wide_since9"]), 60)
        self.assertEqual(len(self.database["wide_since16"]), 5)

    def test_generated_flags_replace_the_handwritten_unicode_tables(self):
        narrow = wide = virama = 0
        for codepoint in range(0x110000):
            flags = property_at(self.database, codepoint)[4]
            narrow += bool(flags & self.generator.FLAG_NARROW_VS15)
            wide += bool(flags & self.generator.FLAG_WIDE_VS16)
            virama += bool(flags & self.generator.FLAG_VIRAMA)
        self.assertEqual((narrow, wide, virama), (150, 213, 41))

    def test_representative_properties_match_unicode_17(self):
        category, width, grapheme, indic, flags = property_at(
            self.database, 0x1F469
        )
        self.assertEqual(
            category,
            self.generator.CATEGORIES.index("So"),
        )
        self.assertEqual(width, 2)
        self.assertEqual(
            grapheme,
            self.generator.GRAPHEME_CLASSES.index("Extended_Pictographic"),
        )
        self.assertEqual(
            indic,
            self.generator.INDIC_CLASSES.index("None"),
        )
        self.assertEqual(flags, 0)

        category, width, grapheme, indic, flags = property_at(
            self.database, 0x094D
        )
        self.assertEqual(
            category,
            self.generator.CATEGORIES.index("Mn"),
        )
        self.assertEqual(width, 0)
        self.assertEqual(
            grapheme,
            self.generator.GRAPHEME_CLASSES.index("Extend"),
        )
        self.assertEqual(
            indic,
            self.generator.INDIC_CLASSES.index("Linker"),
        )
        self.assertTrue(flags & self.generator.FLAG_VIRAMA)

    def test_header_generation_is_deterministic(self):
        self.assertFalse((ROOT / "unicode_data.h").exists())
        self.assertEqual(self.header, self.generator.emit_header(self.database))
        self.assertIn(
            "static constexpr u8 generatedUnicodePageIndices[]",
            self.header,
        )
        self.assertIn(
            "static constexpr GeneratedUnicodeProperty generatedUnicodeProperties[]",
            self.header,
        )


if __name__ == "__main__":
    unittest.main()
