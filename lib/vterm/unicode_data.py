#!/usr/bin/env python3
# Copyright (C) 2026 Shitty team
# MIT licensed
# See the file LICENSE.MIT for the full license.

"""Generate the Unicode properties used by the terminal.

The raw Unicode 17 files are vendored. The generated C++ header is a build
artifact: it contains only the properties consumed by Shitty, compressed as
deduplicated 256-codepoint pages. It also carries the historical East Asian
width deltas and the variation/stacking classifications used for cluster
widths.

    unicode_data.py <unicode-dir> <out.h>
"""

import re
import sys
from pathlib import Path


CODEPOINT_COUNT = 0x110000
PAGE_SIZE = 0x100
UNICODE_VERSION = "17.0.0"
WIDTH_VERSIONS = ("8.0.0", "15.0.0", UNICODE_VERSION)

CATEGORIES = (
    "Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Mc", "Me", "Nd",
    "Nl", "No", "Pc", "Pd", "Ps", "Pe", "Pi", "Pf", "Po", "Sm",
    "Sc", "Sk", "So", "Zs", "Zl", "Zp", "Cc", "Cf", "Cs", "Co",
)
GRAPHEME_CLASSES = (
    "Other", "CR", "LF", "Control", "Extend", "L", "V", "T",
    "LV", "LVT", "Regional_Indicator", "SpacingMark", "Prepend", "ZWJ",
    "Extended_Pictographic",
)
INDIC_CLASSES = ("None", "Linker", "Consonant", "Extend")

FLAG_NARROW_VS15 = 1 << 0
FLAG_WIDE_VS16 = 1 << 1
FLAG_VIRAMA = 1 << 2

RANGE = re.compile(
    r"^([0-9A-F]+)(?:\.\.([0-9A-F]+))?\s*;\s*([^#]+?)\s*(?:#|$)"
)
VARIATION_SEQUENCE = re.compile(
    r"^([0-9A-F]+)\s+FE0[EF]\s*;\s*(?:text|emoji) style"
)


def source_path(directory, stem, version=UNICODE_VERSION):
    return directory / f"{stem}-{version}.txt"


def ranges(path):
    for line in path.read_text().splitlines():
        match = RANGE.match(line)
        if match is None:
            continue
        first = int(match.group(1), 16)
        last = int(match.group(2) or match.group(1), 16)
        yield first, last, match.group(3).strip()


def fill(values, first, last, value):
    values[first:last + 1] = [value] * (last - first + 1)


def read_categories(directory):
    values = ["Cn"] * CODEPOINT_COUNT
    for first, last, category in ranges(
        source_path(directory, "DerivedGeneralCategory")
    ):
        if category not in CATEGORIES:
            raise ValueError(f"unknown general category: {category}")
        fill(values, first, last, category)
    return values


def read_east_asian_width(directory, version):
    assigned = bytearray(CODEPOINT_COUNT)
    width = bytearray(CODEPOINT_COUNT)
    for first, last, category in ranges(
        source_path(directory, "EastAsianWidth", version)
    ):
        for codepoint in range(first, last + 1):
            assigned[codepoint] = 1
            if category in ("W", "F"):
                width[codepoint] = 2
            elif category in ("Na", "H", "A"):
                width[codepoint] = 1
    return assigned, width


def read_grapheme_classes(directory, categories):
    values = ["Other"] * CODEPOINT_COUNT
    # Preserve the previous runtime behavior: default ranges in the UCD must
    # not assign grapheme behavior to unassigned codepoints.
    for first, last, grapheme_class in ranges(
        source_path(directory, "GraphemeBreakProperty")
    ):
        if grapheme_class not in GRAPHEME_CLASSES:
            raise ValueError(f"unknown grapheme class: {grapheme_class}")
        for codepoint in range(first, last + 1):
            if categories[codepoint] != "Cn":
                values[codepoint] = grapheme_class

    for first, last, property_name in ranges(
        source_path(directory, "emoji-data")
    ):
        if property_name == "Extended_Pictographic":
            grapheme_class = "Extended_Pictographic"
        elif property_name == "Emoji_Modifier":
            grapheme_class = "Extend"
        else:
            continue
        for codepoint in range(first, last + 1):
            if categories[codepoint] != "Cn":
                values[codepoint] = grapheme_class
    return values


def read_indic_classes(directory, categories):
    values = ["None"] * CODEPOINT_COUNT
    for first, last, property_name in ranges(
        source_path(directory, "DerivedCoreProperties")
    ):
        if not property_name.startswith("InCB; "):
            continue
        indic_class = property_name.split("; ", 1)[1]
        if indic_class not in INDIC_CLASSES:
            raise ValueError(f"unknown Indic conjunct class: {indic_class}")
        for codepoint in range(first, last + 1):
            if categories[codepoint] != "Cn":
                values[codepoint] = indic_class
    return values


def read_default_ignorable(directory):
    result = set()
    for first, last, property_name in ranges(
        source_path(directory, "DerivedCoreProperties")
    ):
        if property_name == "Default_Ignorable_Code_Point":
            result.update(range(first, last + 1))
    return result


def derive_widths(categories, east_asian_width):
    zero_width_categories = {"Mn", "Mc", "Me", "Zl", "Zp", "Cc", "Cf", "Cs"}
    widths = bytearray([1]) * CODEPOINT_COUNT
    for codepoint, category in enumerate(categories):
        if category == "Cn":
            continue
        width = 0 if category in zero_width_categories else 1
        if east_asian_width[codepoint] != 0:
            width = east_asian_width[codepoint]
        if category == "Mn":
            width = 0
        if codepoint == 0x00AD:
            width = 1
        elif codepoint in (0x2028, 0x2029):
            width = 0
        widths[codepoint] = width
    return widths


def read_variation_bases(directory, widths):
    registered = set()
    for line in source_path(
        directory, "emoji-variation-sequences"
    ).read_text().splitlines():
        match = VARIATION_SEQUENCE.match(line)
        if match is not None:
            registered.add(int(match.group(1), 16))

    emoji_presentation = set()
    for first, last, property_name in ranges(
        source_path(directory, "emoji-data")
    ):
        if property_name == "Emoji_Presentation":
            emoji_presentation.update(range(first, last + 1))

    # These emoji-default CJK symbols have a full-width text presentation.
    # VS15 must not narrow them, because doing so can only crop their glyph.
    full_width_emoji_text = {0x1F21A, 0x1F22F}
    narrow = (registered & emoji_presentation) - full_width_emoji_text
    wide = {
        codepoint for codepoint in registered - emoji_presentation
        if widths[codepoint] == 1
    }
    neutral = registered - narrow - wide
    expected_neutral = {
        0x3030, 0x303D, 0x3297, 0x3299,
        0x1F202, 0x1F21A, 0x1F22F, 0x1F237,
    }
    if neutral != expected_neutral:
        raise ValueError(f"unexpected width-neutral variation bases: {neutral}")
    return narrow, wide


def read_viramas(directory):
    result = set()
    for first, last, property_name in ranges(
        source_path(directory, "IndicSyllabicCategory")
    ):
        if property_name in ("Virama", "Invisible_Stacker"):
            result.update(range(first, last + 1))
    return result


def member_ranges(values):
    result = []
    begin = None
    for codepoint in range(CODEPOINT_COUNT + 1):
        inside = codepoint < CODEPOINT_COUNT and values[codepoint]
        if inside and begin is None:
            begin = codepoint
        elif not inside and begin is not None:
            result.append((begin, codepoint - 1))
            begin = None
    return result


def reclassified(older_assigned, older_width, newer_width):
    gained = bytearray(CODEPOINT_COUNT)
    lost = bytearray(CODEPOINT_COUNT)
    for codepoint in range(CODEPOINT_COUNT):
        if not older_assigned[codepoint]:
            continue
        if newer_width[codepoint] == 2 and older_width[codepoint] != 2:
            gained[codepoint] = 1
        if older_width[codepoint] == 2 and newer_width[codepoint] != 2:
            lost[codepoint] = 1
    return member_ranges(gained), member_ranges(lost)


def deduplicate(values):
    unique_values = list(dict.fromkeys(values))
    value_indices = {value: index for index, value in enumerate(unique_values)}
    indices = [value_indices[value] for value in values]
    pages = [
        tuple(indices[offset:offset + PAGE_SIZE])
        for offset in range(0, CODEPOINT_COUNT, PAGE_SIZE)
    ]
    unique_pages = list(dict.fromkeys(pages))
    page_indices = {page: index for index, page in enumerate(unique_pages)}
    if len(unique_values) > 0x100 or len(unique_pages) > 0x100:
        raise ValueError(
            f"Unicode table no longer fits byte indices: "
            f"{len(unique_values)} properties, {len(unique_pages)} pages"
        )
    return unique_values, unique_pages, [page_indices[page] for page in pages]


def build_database(directory):
    categories = read_categories(directory)
    east_asian = {
        version: read_east_asian_width(directory, version)
        for version in WIDTH_VERSIONS
    }
    grapheme_classes = read_grapheme_classes(directory, categories)
    indic_classes = read_indic_classes(directory, categories)
    widths = derive_widths(categories, east_asian[UNICODE_VERSION][1])
    narrow_variation, wide_variation = read_variation_bases(directory, widths)
    viramas = read_viramas(directory)

    category_indices = {value: index for index, value in enumerate(CATEGORIES)}
    grapheme_indices = {
        value: index for index, value in enumerate(GRAPHEME_CLASSES)
    }
    indic_indices = {value: index for index, value in enumerate(INDIC_CLASSES)}
    values = []
    for codepoint in range(CODEPOINT_COUNT):
        flags = 0
        if codepoint in narrow_variation:
            flags |= FLAG_NARROW_VS15
        if codepoint in wide_variation:
            flags |= FLAG_WIDE_VS16
        if codepoint in viramas:
            flags |= FLAG_VIRAMA
        values.append((
            category_indices[categories[codepoint]],
            widths[codepoint],
            grapheme_indices[grapheme_classes[codepoint]],
            indic_indices[indic_classes[codepoint]],
            flags,
        ))

    unique_values, pages, page_indices = deduplicate(values)
    assigned8, width8 = east_asian["8.0.0"]
    assigned15, width15 = east_asian["15.0.0"]
    width17 = east_asian[UNICODE_VERSION][1]
    wide_since9, narrow_since9 = reclassified(assigned8, width8, width15)
    wide_since16, narrow_since16 = reclassified(
        assigned15, width15, width17
    )
    if narrow_since9 or narrow_since16:
        raise ValueError(
            "unexpected Wide-to-Narrow reclassification: "
            f"{narrow_since9} {narrow_since16}"
        )
    # The visible format controls: Cf characters outside
    # Default_Ignorable_Code_Point. The tables keep them zero-width; the
    # terminal resolves their cell width at startup, because libc
    # implementations disagree about them.
    default_ignorable = read_default_ignorable(directory)
    spacing_formats = [
        codepoint for codepoint in range(CODEPOINT_COUNT)
        if categories[codepoint] == "Cf" and codepoint not in default_ignorable
    ]
    if len(spacing_formats) > 64:
        raise ValueError(
            f"{len(spacing_formats)} visible format controls no longer fit the 64-bit width override mask"
        )
    return {
        "properties": unique_values,
        "pages": pages,
        "page_indices": page_indices,
        "wide_since9": wide_since9,
        "wide_since16": wide_since16,
        "spacing_formats": spacing_formats,
    }


def emit_bytes(output, name, values):
    output.append(f"static constexpr u8 {name}[] = {{")
    for offset in range(0, len(values), 32):
        chunk = values[offset:offset + 32]
        output.append("    " + ", ".join(str(value) for value in chunk) + ",")
    output.append("};")
    output.append("")


def emit_width_ranges(output, name, values):
    output.append(f"static constexpr GeneratedWidthDeltaRange {name}[] = {{")
    for first, last in values:
        output.append(f"    {{0x{first:04x}, 0x{last:04x}}},")
    output.append("};")
    output.append("")


def emit_header(database):
    output = [
        "// Generated by unicode_data.py from the vendored Unicode 17 UCD; do not edit.",
        "#pragma once",
        "",
        "#include <std/sys/types.h>",
        "",
        "struct GeneratedUnicodeProperty {",
        "    u8 category;",
        "    u8 width;",
        "    u8 graphemeClass;",
        "    u8 indicClass;",
        "    u8 flags;",
        "};",
        "",
        "struct GeneratedWidthDeltaRange {",
        "    u32 first;",
        "    u32 last;",
        "};",
        "",
        "static constexpr u32 generatedUnicodeVersion = 17;",
        "static constexpr u32 generatedUnicodePageSize = 0x100;",
        "",
        "static constexpr GeneratedUnicodeProperty generatedUnicodeProperties[] = {",
    ]
    for category, width, grapheme_class, indic_class, flags in database["properties"]:
        output.append(
            f"    {{{category}, {width}, {grapheme_class}, {indic_class}, {flags}}},"
        )
    output.append("};")
    output.append("")
    emit_bytes(output, "generatedUnicodePageIndices", database["page_indices"])
    flattened_pages = [value for page in database["pages"] for value in page]
    emit_bytes(output, "generatedUnicodePropertyIndices", flattened_pages)
    emit_width_ranges(output, "generatedWideSince9", database["wide_since9"])
    emit_width_ranges(output, "generatedWideSince16", database["wide_since16"])
    output.append("static constexpr u32 generatedSpacingFormatControls[] = {")
    for offset in range(0, len(database["spacing_formats"]), 8):
        chunk = database["spacing_formats"][offset:offset + 8]
        output.append("    " + ", ".join(f"0x{value:04x}" for value in chunk) + ",")
    output.append("};")
    output.append("")
    return "\n".join(output) + "\n"


def generate(directory):
    return emit_header(build_database(directory))


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: unicode_data.py <unicode-dir> <out.h>")
    directory = Path(sys.argv[1])
    Path(sys.argv[2]).write_text(generate(directory))


if __name__ == "__main__":
    main()
