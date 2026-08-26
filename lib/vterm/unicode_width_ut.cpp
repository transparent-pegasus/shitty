/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "unicode.h"
#include "unicode_width.h"

#include <std/tst/ut.h>

#include <wchar.h>

using namespace stl;

STD_TEST_SUITE(UnicodeWidth) {
    STD_TEST(ReportsRepresentativeCodepointWidths) {
        const UnicodeWidths full(0);
        STD_INSIST(full.codepointWidth('A') == 1);
        STD_INSIST(full.codepointWidth(0x0301) == 0);
        STD_INSIST(full.codepointWidth(0x1160) == 0);
        STD_INSIST(full.codepointWidth(0x11ff) == 0);
        STD_INSIST(full.codepointWidth(0x4e00) == 2);
        STD_INSIST(full.codepointWidth(0x1f1fa) == 2);
        STD_INSIST(full.codepointWidth(0) == 0);
    }

    // U+231A went Wide with the Unicode 9 emoji batch, U+2632 with the
    // 15.1 trigram batch; a lowered level answers like the libcs that
    // predate them, and ideographs stay wide at every level.
    STD_TEST(WidthLevelUndoesReclassifications) {
        STD_INSIST(UnicodeWidths(0).codepointWidth(0x231a) == 2);
        STD_INSIST(UnicodeWidths(15).codepointWidth(0x231a) == 2);
        STD_INSIST(UnicodeWidths(8).codepointWidth(0x231a) == 1);
        STD_INSIST(UnicodeWidths(0).codepointWidth(0x2632) == 2);
        STD_INSIST(UnicodeWidths(15).codepointWidth(0x2632) == 1);
        STD_INSIST(UnicodeWidths(8).codepointWidth(0x2632) == 1);
        STD_INSIST(UnicodeWidths(8).codepointWidth(0x4e00) == 2);
    }

    // The visible format controls - Cf outside Default_Ignorable - are
    // where libcs disagree: glibc gives them a cell, musl does not. The
    // constructor asks this system's wcwidth, so its answers are the
    // expectation, and the level axis must not disturb them.
    STD_TEST(FormatControlWidthsFollowTheSystemLibc) {
        size_t count = 0;
        const u32* const controls = unicodeSpacingFormatControls(count);
        STD_INSIST(count > 0);
        const UnicodeWidths full(0);
        const UnicodeWidths lowered(8);
        for (size_t index = 0; index < count; ++index) {
            const int expected = wcwidth((wchar_t)(controls[index])) > 0 ? 1 : 0;
            STD_INSIST(full.codepointWidth(controls[index]) == expected);
            STD_INSIST(lowered.codepointWidth(controls[index]) == expected);
        }
    }

    // Format characters outside the disputed list keep their table
    // widths: the default-ignorable ones stay invisible everywhere, and
    // the soft hyphen keeps its one historical cell.
    STD_TEST(DefaultIgnorableFormatsStayZeroWidth) {
        const UnicodeWidths full(0);
        STD_INSIST(full.codepointWidth(0x200b) == 0);
        STD_INSIST(full.codepointWidth(0x200d) == 0);
        STD_INSIST(full.codepointWidth(0x2060) == 0);
        STD_INSIST(full.codepointWidth(0x00ad) == 1);
    }

    STD_TEST(AppliesVariationSelectorWidthOverrides) {
        STD_INSIST(UnicodeWidths(0).graphemeWidthEffect(0x00a9, 0xfe0f) == GraphemeWidthEffect::Wide);
        STD_INSIST(UnicodeWidths(0).graphemeWidthEffect(0x231a, 0xfe0e) == GraphemeWidthEffect::Narrow);
        STD_INSIST(UnicodeWidths(0).graphemeWidthEffect('A', 0xfe0f) == GraphemeWidthEffect::Unchanged);

        // Text-default CJK symbols and the Enclosed Ideographic Supplement
        // keep their full width under VS15: their text glyph is a
        // full-width form, so narrowing can only crop it (the
        // unicode-width rule; the contour mode-2027 spec says VS15 must
        // not change width at all).
        static constexpr u32 fullWidthTextBases[] = {0x3030, 0x303d, 0x3297, 0x3299, 0x1f202, 0x1f21a, 0x1f22f, 0x1f237};
        for (const u32 base : fullWidthTextBases) {
            STD_INSIST(UnicodeWidths(0).graphemeWidthEffect(base, 0xfe0e) == GraphemeWidthEffect::Unchanged);
        }
    }

    STD_TEST(DistinguishesSpacingMarksAndViramas) {
        STD_INSIST(UnicodeWidths(0).graphemeWidthEffect(0x0915, 0x093e) == GraphemeWidthEffect::Wide);
        STD_INSIST(UnicodeWidths(0).graphemeWidthEffect(0x0915, 0x094d) == GraphemeWidthEffect::Unchanged);
        STD_INSIST(UnicodeWidths(0).graphemeWidthEffect('a', 0x0301) == GraphemeWidthEffect::Unchanged);
    }
}
