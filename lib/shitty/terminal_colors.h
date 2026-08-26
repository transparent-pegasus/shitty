/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <lib/vterm/ansi_palette.h>

#include <std/str/view.h>

struct TerminalColorScheme {
    const char* name;
    u8 foreground[3];
    u8 background[3];
    u8 ansi[AnsiPalette::colorCount][3];

    Color foregroundColor() const noexcept;
    Color backgroundColor() const noexcept;
    AnsiPalette ansiPalette() const noexcept;

    static const TerminalColorScheme* find(stl::StringView name) noexcept;
    static const TerminalColorScheme* all() noexcept;
    static size_t count() noexcept;
    static const TerminalColorScheme* builtins() noexcept;
    static size_t builtinCount() noexcept;
};
