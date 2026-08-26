/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/str/view.h>
#include <std/sys/types.h>

struct Composer;
struct Vterm;

// The -debug trace: an append-only file of window, font and grid
// events, timestamped from process start. It exists so a bug we cannot
// reproduce - the macOS fullscreen transitions of issues 83 and 86 -
// can be run by its reporter and read back as evidence. Inert unless
// the option names a file.
void openDebugTrace(Composer& composer);
void debugTraceLine(Composer& composer, stl::StringView line);
// One line of model-side evidence for the terminal: grid, history,
// view offset and a content checksum - enough to tell a screen that
// lost its cells from a renderer that stopped drawing them.
void debugTraceTerminal(Composer& composer, stl::StringView what, Vterm& terminal);
