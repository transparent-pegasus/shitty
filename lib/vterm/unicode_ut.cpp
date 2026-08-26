/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "unicode.h"

#include <std/tst/ut.h>

using namespace stl;

STD_TEST_SUITE(Unicode) {
    STD_TEST(ReportsBundledVersion) {
        STD_INSIST(unicodeVersion() == 17);
    }

    STD_TEST(ReportsGeneralCategories) {
        STD_INSIST(unicodeCodepointProperties('A').category == GeneralCategory::UppercaseLetter);
        STD_INSIST(unicodeCodepointProperties(0x0301).category == GeneralCategory::NonspacingMark);
        STD_INSIST(unicodeCodepointProperties(0x3000).category == GeneralCategory::SpaceSeparator);
        STD_INSIST(unicodeCodepointProperties(0x10ffff).category == GeneralCategory::Unassigned);
    }

    STD_TEST(ReportsGraphemeAndIndicProperties) {
        const UnicodeCodepointProperties emoji = unicodeCodepointProperties(0x1f469);
        STD_INSIST(emoji.graphemeClass == GraphemeClass::ExtendedPictographic);
        STD_INSIST(emoji.indicConjunctClass == IndicConjunctClass::None);

        const UnicodeCodepointProperties virama = unicodeCodepointProperties(0x094d);
        STD_INSIST(virama.graphemeClass == GraphemeClass::Extend);
        STD_INSIST(virama.indicConjunctClass == IndicConjunctClass::Linker);
        STD_INSIST(virama.virama);
    }

    STD_TEST(ReportsVariationWidthPolicy) {
        STD_INSIST(unicodeCodepointProperties(0x231a).narrowsWithVs15);
        STD_INSIST(unicodeCodepointProperties(0x0023).widensWithVs16);
        STD_INSIST(!unicodeCodepointProperties(0x3030).narrowsWithVs15);
        STD_INSIST(!unicodeCodepointProperties(0x3030).widensWithVs16);
    }

    STD_TEST(RejectsOutOfRangeAsUnassigned) {
        const UnicodeCodepointProperties property = unicodeCodepointProperties(0x110000);
        STD_INSIST(property.category == GeneralCategory::Unassigned);
        STD_INSIST(property.graphemeClass == GraphemeClass::Other);
        STD_INSIST(property.indicConjunctClass == IndicConjunctClass::None);
        STD_INSIST(property.width == 1);
    }
}
