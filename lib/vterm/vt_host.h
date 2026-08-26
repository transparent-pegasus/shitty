/*
 * Copyright (C) 2026 Shitty team
 * MIT licensed
 * See the file LICENSE.MIT for the full license.
 */

#pragma once

#include <std/str/view.h>
#include <std/sys/types.h>

#include <plt/window.h>

struct VtermTitleChanged;

// Every way the terminal reaches past pure byte-stream semantics: the
// clipboards OSC 52 and the selection write, the window operations
// XTWINOPS drives, the frame request that publishes damage, the desktop
// actions of hyperlinks, the title publication, and the resize echo of
// an in-band grid change. The embedder implements it: the GUI adapter
// forwards to the platform window and fans events into its listener
// lists, the headless host emulates against its headless window.
struct VtHost {
    virtual plt::Clipboard* primary() = 0;
    virtual plt::Clipboard* secondary() = 0;
    virtual plt::WindowInfo info() = 0;
    virtual void requestFrame() = 0;
    virtual void requestResize(u32 width, u32 height) = 0;
    virtual void requestMaximized(bool maximized) = 0;
    virtual void requestFullscreen(bool fullscreen) = 0;
    virtual void requestIconify() = 0;
    virtual void requestRestore() = 0;
    virtual void requestMove(i32 x, i32 y) = 0;
    virtual void requestFocus() = 0;
    virtual void requestAttention() = 0;
    virtual void requestPointerIcon(plt::PointerIcon icon) = 0;
    virtual void requestOpenUri(stl::StringView uri) = 0;
    // Whether a detected plain-text URI with this scheme is actionable.
    // The scheme list is the embedder's policy; an explicit OSC 8 link
    // is authoritative and never asks.
    virtual bool uriSchemeAllowed(stl::StringView scheme) = 0;
    // A terminal published its undecorated title; the embedder decides
    // whether the source is visible and how a window presents it.
    virtual void titleChanged(const VtermTitleChanged& event) = 0;
    // The grid geometry moved under an in-band resize the core applied
    // itself; every terminal behind the window must hear it.
    virtual void resized() = 0;
};
