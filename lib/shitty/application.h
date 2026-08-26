/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

struct Composer;

struct Application {
    virtual int run(int argc, char* argv[]) = 0;

    static Application* create(Composer& composer);
};

// The startup window-state request from the parsed options, shared by
// the interactive run and the test-mode driver: fullscreen wins over
// maximized - it is the stronger request, and leaving the pair
// unresolved would let every window manager settle it differently.
void applyStartupWindowState(Composer& composer);
