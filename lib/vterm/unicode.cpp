/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#include "unicode.h"

#include "unicode_data.h"

namespace {
    static constexpr u8 narrowVs15 = 1 << 0;
    static constexpr u8 wideVs16 = 1 << 1;
    static constexpr u8 virama = 1 << 2;

    template <size_t Size>
    static bool contains(const GeneratedWidthDeltaRange (&ranges)[Size], u32 codepoint) {
        size_t first = 0;
        size_t last = Size;
        while (first < last) {
            const size_t middle = (first + last) / 2;
            if (ranges[middle].last < codepoint) {
                first = middle + 1;
            } else {
                last = middle;
            }
        }
        return first < Size && ranges[first].first <= codepoint;
    }
}

static_assert((u8)(GeneralCategory::Unassigned) == 0);
static_assert((u8)(GeneralCategory::PrivateUse) == 29);
static_assert((u8)(GraphemeClass::Other) == 0);
static_assert((u8)(GraphemeClass::ExtendedPictographic) == 14);
static_assert((u8)(IndicConjunctClass::None) == 0);
static_assert((u8)(IndicConjunctClass::Extend) == 3);
static_assert(sizeof(GeneratedUnicodeProperty) == 5);
static_assert(sizeof(generatedUnicodePageIndices) == 0x1100);

UnicodeCodepointProperties unicodeCodepointProperties(u32 codepoint) {
    if (codepoint >= 0x110000) {
        return {
            .category = GeneralCategory::Unassigned,
            .graphemeClass = GraphemeClass::Other,
            .indicConjunctClass = IndicConjunctClass::None,
            .width = 1,
            .narrowsWithVs15 = false,
            .widensWithVs16 = false,
            .virama = false,
        };
    }
    const u32 page = generatedUnicodePageIndices[codepoint / generatedUnicodePageSize];
    const u32 propertyIndex = generatedUnicodePropertyIndices[page * generatedUnicodePageSize + codepoint % generatedUnicodePageSize];
    const GeneratedUnicodeProperty& property = generatedUnicodeProperties[propertyIndex];
    return {
        .category = (GeneralCategory)(property.category),
        .graphemeClass = (GraphemeClass)(property.graphemeClass),
        .indicConjunctClass = (IndicConjunctClass)(property.indicClass),
        .width = property.width,
        .narrowsWithVs15 = (property.flags & narrowVs15) != 0,
        .widensWithVs16 = (property.flags & wideVs16) != 0,
        .virama = (property.flags & virama) != 0,
    };
}

u32 unicodeVersion() {
    return generatedUnicodeVersion;
}

bool unicodeWideSince9(u32 codepoint) {
    return contains(generatedWideSince9, codepoint);
}

bool unicodeWideSince16(u32 codepoint) {
    return contains(generatedWideSince16, codepoint);
}

static_assert(sizeof(generatedSpacingFormatControls) / sizeof(u32) <= 64);

const u32* unicodeSpacingFormatControls(size_t& count) {
    count = sizeof(generatedSpacingFormatControls) / sizeof(u32);
    return generatedSpacingFormatControls;
}
