/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/sys/types.h>

#include <stddef.h>

enum class GeneralCategory : u8 {
    Unassigned,
    UppercaseLetter,
    LowercaseLetter,
    TitlecaseLetter,
    ModifierLetter,
    OtherLetter,
    NonspacingMark,
    SpacingMark,
    EnclosingMark,
    DecimalNumber,
    LetterNumber,
    OtherNumber,
    ConnectorPunctuation,
    DashPunctuation,
    OpenPunctuation,
    ClosePunctuation,
    InitialPunctuation,
    FinalPunctuation,
    OtherPunctuation,
    MathSymbol,
    CurrencySymbol,
    ModifierSymbol,
    OtherSymbol,
    SpaceSeparator,
    LineSeparator,
    ParagraphSeparator,
    Control,
    Format,
    Surrogate,
    PrivateUse,
};

enum class GraphemeClass : u8 {
    Other,
    Cr,
    Lf,
    Control,
    Extend,
    HangulL,
    HangulV,
    HangulT,
    HangulLv,
    HangulLvt,
    RegionalIndicator,
    SpacingMark,
    Prepend,
    Zwj,
    ExtendedPictographic,
    EmojiZwj,
};

enum class IndicConjunctClass : u8 {
    None,
    Linker,
    Consonant,
    Extend,
};

struct UnicodeCodepointProperties {
    GeneralCategory category;
    GraphemeClass graphemeClass;
    IndicConjunctClass indicConjunctClass;
    u8 width;
    bool narrowsWithVs15;
    bool widensWithVs16;
    bool virama;
};

UnicodeCodepointProperties unicodeCodepointProperties(u32 codepoint);
u32 unicodeVersion();
bool unicodeWideSince9(u32 codepoint);
bool unicodeWideSince16(u32 codepoint);
// The visible format controls: Format codepoints outside
// Default_Ignorable_Code_Point, whose cell width libc implementations
// disagree about. Sorted ascending; the property tables keep them
// zero-width and UnicodeWidths overrides them by list position.
const u32* unicodeSpacingFormatControls(size_t& count);
